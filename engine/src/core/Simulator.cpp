#include "logicbench/Simulator.h"

#include "logicbench/IoComponents.h"

#include <algorithm>
#include <limits>

namespace lb {

namespace {

/// Every component that lies on a combinational cycle, found with Tarjan's
/// strongly-connected-components algorithm.
///
/// Edges leave a component's output pins and arrive at the components its
/// wires feed. Sequential parts contribute no outgoing edges, so a path
/// through a flip-flop is not a combinational cycle and a counter is not
/// mistaken for a ring oscillator.
///
/// The walk is iterative rather than recursive: a thousand-gate chain is an
/// ordinary circuit here, and that is deep enough to matter.
std::unordered_set<ComponentId> findCombinationalCycles(const Circuit& circuit)
{
    std::unordered_map<ComponentId, std::vector<ComponentId>> adjacency;
    for (const std::unique_ptr<Component>& c : circuit.components()) {
        if (c->isSequential())
            continue;
        std::vector<ComponentId>& edges = adjacency[c->id()];
        for (std::size_t p = 0; p < c->pinCount(); ++p) {
            if (!c->pin(p).isOutput())
                continue;
            for (const PinRef& sink : circuit.sinksOf(PinRef{c->id(), static_cast<int>(p)}))
                edges.push_back(sink.component);
        }
    }

    struct Frame {
        ComponentId node;
        std::size_t nextEdge;
    };

    static const std::vector<ComponentId> kNoEdges;
    const auto edgesOf = [&adjacency](ComponentId node) -> const std::vector<ComponentId>& {
        auto it = adjacency.find(node);
        return it == adjacency.end() ? kNoEdges : it->second;
    };

    std::unordered_map<ComponentId, int> index;
    std::unordered_map<ComponentId, int> lowlink;
    std::unordered_set<ComponentId> onStack;
    std::vector<ComponentId> stack;
    std::vector<Frame> callStack;
    std::unordered_set<ComponentId> cycles;
    int counter = 0;

    for (const std::unique_ptr<Component>& component : circuit.components()) {
        const ComponentId root = component->id();
        if (index.count(root) != 0)
            continue;

        index[root] = lowlink[root] = counter++;
        stack.push_back(root);
        onStack.insert(root);
        callStack.push_back({root, 0});

        while (!callStack.empty()) {
            const ComponentId node = callStack.back().node;
            const std::vector<ComponentId>& edges = edgesOf(node);

            if (callStack.back().nextEdge < edges.size()) {
                const ComponentId next = edges[callStack.back().nextEdge++];
                if (index.count(next) == 0) {
                    index[next] = lowlink[next] = counter++;
                    stack.push_back(next);
                    onStack.insert(next);
                    callStack.push_back({next, 0});
                } else if (onStack.count(next) != 0) {
                    lowlink[node] = std::min(lowlink[node], index[next]);
                }
                continue;
            }

            if (lowlink[node] == index[node]) {
                std::vector<ComponentId> group;
                ComponentId popped = kNoComponent;
                do {
                    popped = stack.back();
                    stack.pop_back();
                    onStack.erase(popped);
                    group.push_back(popped);
                } while (popped != node);

                const bool selfLoop =
                    std::find(edges.begin(), edges.end(), node) != edges.end();
                if (group.size() > 1 || selfLoop)
                    cycles.insert(group.begin(), group.end());
            }

            callStack.pop_back();
            if (!callStack.empty()) {
                ComponentId parent = callStack.back().node;
                lowlink[parent] = std::min(lowlink[parent], lowlink[node]);
            }
        }
    }

    return cycles;
}

} // namespace

Simulator::Simulator(Circuit& circuit)
    : m_circuit(circuit)
{
    refreshClocks();
}

void Simulator::push(ComponentId id, Tick at)
{
    m_queue.push(Event{at, m_sequence++, id});
}

void Simulator::schedule(ComponentId id)
{
    if (Component* c = m_circuit.component(id))
        push(id, m_now + static_cast<Tick>(c->delay()));
}

void Simulator::scheduleAll()
{
    for (const std::unique_ptr<Component>& c : m_circuit.components())
        push(c->id(), m_now);
}

void Simulator::refreshClocks()
{
    std::unordered_map<ComponentId, Tick> refreshed;
    for (Component* c : m_circuit.componentsOfType("CLOCK")) {
        auto* clock = static_cast<Clock*>(c);
        const Tick half = static_cast<Tick>(std::max(1, clock->period() / 2));
        auto existing = m_nextClockFlip.find(c->id());
        refreshed[c->id()] = (existing != m_nextClockFlip.end() && existing->second > m_now)
                                 ? existing->second
                                 : m_now + half;
    }
    m_nextClockFlip.swap(refreshed);
}

SettleReport Simulator::reset()
{
    m_queue = {};
    m_now = 0;
    m_sequence = 0;
    m_evaluations.clear();
    m_stopped.clear();
    m_lastOscillating.clear();
    m_nextClockFlip.clear();
    m_circuit.reset();
    refreshClocks();
    scheduleAll();
    return settle();
}

void Simulator::propagate(Component& source)
{
    for (std::size_t p = 0; p < source.pinCount(); ++p) {
        const Pin& out = source.pin(p);
        if (!out.isOutput())
            continue;
        const PinRef ref{source.id(), static_cast<int>(p)};
        for (const PinRef& sink : m_circuit.sinksOf(ref)) {
            Component* target = m_circuit.component(sink.component);
            if (!target)
                continue;
            Pin& in = target->pin(sink.pin);
            if (in.signal() == out.signal())
                continue;                    // no change, no wake-up
            in.setSignal(out.signal());
            push(target->id(), m_now + static_cast<Tick>(target->delay()));
        }
    }
}

void Simulator::forceUndefined(Component& component)
{
    for (std::size_t p = 0; p < component.pinCount(); ++p) {
        if (component.pin(p).isOutput())
            component.pin(p).setSignal(Signal::Undefined);
    }
    propagate(component);
}

const std::unordered_set<ComponentId>& Simulator::cycleMembers()
{
    if (m_cycleVersion != m_circuit.topologyVersion()) {
        m_cycleMembers = findCombinationalCycles(m_circuit);
        m_cycleVersion = m_circuit.topologyVersion();
    }
    return m_cycleMembers;
}

void Simulator::flagUnresolvedCycles(SettleReport& report)
{
    const std::unordered_set<ComponentId>& cycles = cycleMembers();
    if (cycles.empty())
        return;

    for (const std::unique_ptr<Component>& c : m_circuit.components()) {
        if (cycles.count(c->id()) == 0)
            continue;
        if (std::find(m_lastOscillating.begin(), m_lastOscillating.end(), c->id())
            != m_lastOscillating.end()) {
            continue;                    // already caught by the budget
        }

        bool undefinedOutput = false;
        for (std::size_t p = 0; p < c->pinCount(); ++p) {
            if (c->pin(p).isOutput() && c->pin(p).signal() == Signal::Undefined) {
                undefinedOutput = true;
                break;
            }
        }
        // A cycle that carries definite levels has resolved — an SR latch that
        // has been set, for instance. Only an undefined one is a loop the user
        // needs told about.
        if (undefinedOutput)
            m_lastOscillating.push_back(c->id());
    }

    report.oscillating = m_lastOscillating;
}

SettleReport Simulator::settle()
{
    return settleUntil(std::numeric_limits<Tick>::max());
}

SettleReport Simulator::settleUntil(Tick horizon)
{
    SettleReport report;
    m_evaluations.clear();
    m_stopped.clear();
    m_lastOscillating.clear();

    while (!m_queue.empty() && m_queue.top().time <= horizon) {
        if (report.eventsProcessed >= m_eventCap) {
            report.settled = false;         // pathological circuit; give up cleanly
            break;
        }

        const Event event = m_queue.top();
        m_queue.pop();
        ++report.eventsProcessed;
        m_now = std::max(m_now, event.time);

        Component* component = m_circuit.component(event.component);
        if (!component)
            continue;                        // deleted since the event was queued
        if (m_stopped.count(event.component) != 0)
            continue;                        // already flagged as oscillating

        const std::size_t count = ++m_evaluations[event.component];
        if (count > m_budget) {
            // It has been re-evaluated far more often than any settling
            // circuit needs. Treat the value as unknowable, say so, and stop
            // feeding the loop.
            m_stopped.insert(event.component);
            m_lastOscillating.push_back(event.component);
            forceUndefined(*component);
            continue;
        }

        if (component->evaluate())
            propagate(*component);
    }

    report.oscillating = m_lastOscillating;
    flagUnresolvedCycles(report);
    return report;
}

SettleReport Simulator::evaluateFrom(ComponentId id)
{
    schedule(id);
    return settle();
}

SettleReport Simulator::advance(Tick ticks, const std::function<void(Tick)>& onTick)
{
    SettleReport total;

    // An unbounded settle can have carried simulated time past a clock's next
    // flip; bring the schedule forward rather than firing the missed flips in
    // a burst.
    for (auto& entry : m_nextClockFlip) {
        if (entry.second > m_now)
            continue;
        Component* c = m_circuit.component(entry.first);
        if (!c)
            continue;
        const auto* clock = static_cast<const Clock*>(c);
        entry.second = m_now + static_cast<Tick>(std::max(1, clock->period() / 2));
    }

    for (Tick i = 0; i < ticks; ++i) {
        const Tick target = m_now + 1;
        m_now = target;

        for (auto& entry : m_nextClockFlip) {
            Component* c = m_circuit.component(entry.first);
            if (!c)
                continue;
            auto* clock = static_cast<Clock*>(c);
            if (!clock->running())
                continue;
            const Tick half = static_cast<Tick>(std::max(1, clock->period() / 2));
            while (entry.second <= target) {
                if (clock->flip())
                    propagate(*clock);
                entry.second += half;
            }
        }

        // Only what is due by now: a delayed gate answers on a later tick,
        // which is what makes the timing diagram show propagation at all.
        const SettleReport step = settleUntil(target);
        m_now = target;
        total.eventsProcessed += step.eventsProcessed;
        total.settled = total.settled && step.settled;
        total.oscillating.insert(total.oscillating.end(),
                                 step.oscillating.begin(), step.oscillating.end());

        if (onTick)
            onTick(m_now);
    }

    std::sort(total.oscillating.begin(), total.oscillating.end());
    total.oscillating.erase(std::unique(total.oscillating.begin(), total.oscillating.end()),
                            total.oscillating.end());
    return total;
}

} // namespace lb
