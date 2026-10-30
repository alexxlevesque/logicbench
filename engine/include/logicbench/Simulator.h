#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/Pin.h"

#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lb {

using Tick = std::uint64_t;

/// What happened during one settle.
struct SettleReport {
    bool settled = true;                      ///< false if the event cap was hit
    std::size_t eventsProcessed = 0;
    std::vector<ComponentId> oscillating;     ///< components the loop detector stopped

    bool stable() const noexcept { return settled && oscillating.empty(); }
};

/// Event-driven simulation over a Circuit.
///
/// A component is evaluated only when one of its inputs has changed, and its
/// readers are woken one propagation delay later. That is what keeps a toggle
/// cheap in a large circuit: the work is proportional to the part of the
/// circuit that actually changed, not to its size.
///
/// Feedback needs two different defences, because it fails in two ways.
///
/// A loop carrying definite levels flips forever, so every component gets an
/// evaluation budget per settle; one that burns through it is declared
/// oscillating, its outputs are forced to Undefined and it stops propagating.
///
/// A loop that never gets a definite level instead sits at Undefined, which is
/// arithmetically stable — the event queue drains and nothing looks wrong. So
/// after settling, any component that lies on a combinational cycle and still
/// has an undefined output is flagged too. That distinction matters: it is
/// what separates a ring oscillator from a merely floating input, and it
/// leaves a working SR latch alone, since a latch that has resolved carries
/// definite levels.
///
/// Cycles through a flip-flop do not count: a sequential part breaks the
/// combinational path, which is why a counter is not reported as a loop.
class Simulator {
public:
    static constexpr std::size_t kDefaultEvaluationBudget = 200;
    static constexpr std::size_t kDefaultEventCap = 2'000'000;

    explicit Simulator(Circuit& circuit);

    Circuit& circuit() noexcept { return m_circuit; }
    const Circuit& circuit() const noexcept { return m_circuit; }

    Tick now() const noexcept { return m_now; }

    /// Clears state, returns time to zero and evaluates the whole circuit once
    /// so that a freshly loaded file shows correct levels before any input is
    /// touched.
    SettleReport reset();

    /// Queues a component for evaluation after its own delay.
    void schedule(ComponentId id);

    /// Queues every component. Used after a load, or after a structural edit
    /// whose full effect is easier to recompute than to track.
    void scheduleAll();

    /// Runs queued events until nothing is left to do, however far into
    /// simulated time that reaches. This is what an edit wants: show the
    /// steady state now.
    SettleReport settle();

    /// Runs only the events due at or before `horizon`, leaving later ones
    /// queued, and parks the clock there. This is what advancing time wants:
    /// a gate with a delay of one tick must respond on the next tick, not
    /// inside this one.
    SettleReport settleUntil(Tick horizon);

    /// Schedules the component, then settles. The common case after an edit.
    SettleReport evaluateFrom(ComponentId id);

    /// Advances virtual time, flipping running clocks on their half-period
    /// boundaries and settling after each tick.
    /// onTick, if set, is called after each tick has settled — the timing
    /// diagram uses it to sample signals.
    SettleReport advance(Tick ticks, const std::function<void(Tick)>& onTick = {});

    /// Ids flagged by the loop detector during the last settle.
    const std::vector<ComponentId>& oscillatingComponents() const noexcept
    {
        return m_lastOscillating;
    }

    void setEvaluationBudget(std::size_t budget) noexcept { m_budget = budget; }
    void setEventCap(std::size_t cap) noexcept { m_eventCap = cap; }

    /// Rebuilds clock bookkeeping. Call after adding or removing a clock, or
    /// after changing a clock's period.
    void refreshClocks();

private:
    struct Event {
        Tick time = 0;
        std::uint64_t sequence = 0;
        ComponentId component = kNoComponent;
    };

    struct LaterFirst {
        bool operator()(const Event& a, const Event& b) const noexcept
        {
            return a.time != b.time ? a.time > b.time : a.sequence > b.sequence;
        }
    };

    void push(ComponentId id, Tick at);
    void propagate(Component& source);
    void forceUndefined(Component& component);
    void flagUnresolvedCycles(SettleReport& report);

    /// Components lying on a combinational cycle, recomputed only when the
    /// circuit's shape changes.
    const std::unordered_set<ComponentId>& cycleMembers();

    Circuit& m_circuit;
    std::priority_queue<Event, std::vector<Event>, LaterFirst> m_queue;
    std::unordered_map<ComponentId, std::size_t> m_evaluations;
    std::unordered_set<ComponentId> m_stopped;
    std::vector<ComponentId> m_lastOscillating;
    std::unordered_map<ComponentId, Tick> m_nextClockFlip;
    std::unordered_set<ComponentId> m_cycleMembers;
    std::uint64_t m_cycleVersion = 0;
    Tick m_now = 0;
    std::uint64_t m_sequence = 0;
    std::size_t m_budget = kDefaultEvaluationBudget;
    std::size_t m_eventCap = kDefaultEventCap;
};

} // namespace lb
