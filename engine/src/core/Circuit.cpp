#include "logicbench/Circuit.h"

#include <algorithm>

namespace lb {

Circuit::Circuit(const Circuit& other)
    : m_name(other.m_name)
    , m_wires(other.m_wires)
    , m_nextComponentId(other.m_nextComponentId)
    , m_nextWireId(other.m_nextWireId)
{
    m_components.reserve(other.m_components.size());
    for (const std::unique_ptr<Component>& c : other.m_components)
        m_components.push_back(c->clone());   // ids ride along with the clone
    reindex();
    rebuildWireIndex();
}

Circuit& Circuit::operator=(const Circuit& other)
{
    if (this != &other) {
        Circuit copy(other);                  // copy and swap: exception safe,
        std::swap(m_name, copy.m_name);       // and self-assignment is a no-op
        std::swap(m_components, copy.m_components);
        std::swap(m_index, copy.m_index);
        std::swap(m_wires, copy.m_wires);
        std::swap(m_fanout, copy.m_fanout);
        std::swap(m_driver, copy.m_driver);
        std::swap(m_nextComponentId, copy.m_nextComponentId);
        std::swap(m_nextWireId, copy.m_nextWireId);
        ++m_topologyVersion;
    }
    return *this;
}

void Circuit::reindex()
{
    m_index.clear();
    m_index.reserve(m_components.size());
    for (const std::unique_ptr<Component>& c : m_components)
        m_index[c->id()] = c.get();
}

void Circuit::rebuildWireIndex()
{
    m_fanout.clear();
    m_driver.clear();
    for (const Wire& w : m_wires) {
        m_fanout[pinKey(w.from)].push_back(w.to);
        m_driver[pinKey(w.to)] = w.id;
    }
}

ComponentId Circuit::add(std::unique_ptr<Component> component, ComponentId preferredId)
{
    if (!component)
        return kNoComponent;

    ComponentId id = preferredId;
    if (id == kNoComponent || m_index.count(id) != 0)
        id = m_nextComponentId;
    while (m_index.count(id) != 0)
        ++id;

    component->setId(id);
    m_index[id] = component.get();
    m_components.push_back(std::move(component));
    m_nextComponentId = std::max(m_nextComponentId, id + 1);
    ++m_topologyVersion;
    return id;
}

std::unique_ptr<Component> Circuit::take(ComponentId id)
{
    auto indexed = m_index.find(id);
    if (indexed == m_index.end())
        return nullptr;

    // Wires first: a component must never be destroyed while a wire still
    // names it, or a later lookup resolves to a hole.
    m_wires.erase(std::remove_if(m_wires.begin(), m_wires.end(),
                                 [id](const Wire& w) {
                                     return w.from.component == id || w.to.component == id;
                                 }),
                  m_wires.end());
    rebuildWireIndex();

    // Anything that was being driven by the departing component is floating
    // again, so put its input pins back to Undefined rather than leaving a
    // stale level behind.
    for (const std::unique_ptr<Component>& c : m_components) {
        if (c->id() == id)
            continue;
        for (std::size_t p = 0; p < c->pinCount(); ++p) {
            if (c->pin(p).isInput() && driverOf(PinRef{c->id(), static_cast<int>(p)}) == nullptr)
                c->pin(p).setSignal(Signal::Undefined);
        }
    }

    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [id](const std::unique_ptr<Component>& c) { return c->id() == id; });
    std::unique_ptr<Component> owned = std::move(*it);
    m_components.erase(it);
    m_index.erase(indexed);
    ++m_topologyVersion;
    return owned;
}

Component* Circuit::component(ComponentId id) const
{
    auto it = m_index.find(id);
    return it == m_index.end() ? nullptr : it->second;
}

void Circuit::validateEndpoints(PinRef from, PinRef to) const
{
    Component* source = component(from.component);
    Component* sink = component(to.component);
    if (!source || !sink)
        throw IllegalConnection("connection refers to a component that is not in the circuit");
    if (from.pin < 0 || static_cast<std::size_t>(from.pin) >= source->pinCount())
        throw IllegalConnection("source pin index is out of range");
    if (to.pin < 0 || static_cast<std::size_t>(to.pin) >= sink->pinCount())
        throw IllegalConnection("destination pin index is out of range");
    if (!source->pin(from.pin).isOutput())
        throw IllegalConnection("a wire must start at an output pin");
    if (!sink->pin(to.pin).isInput())
        throw IllegalConnection("a wire must end at an input pin");
    if (driverOf(to) != nullptr)
        throw IllegalConnection("that input pin is already driven");
}

WireId Circuit::connect(PinRef from, PinRef to, WireId preferredId)
{
    validateEndpoints(from, to);

    WireId id = preferredId;
    const auto taken = [this](WireId candidate) {
        return std::any_of(m_wires.begin(), m_wires.end(),
                           [candidate](const Wire& w) { return w.id == candidate; });
    };
    if (id == kNoWire || taken(id))
        id = m_nextWireId;
    while (taken(id))
        ++id;

    m_wires.push_back(Wire{id, from, to});
    m_fanout[pinKey(from)].push_back(to);
    m_driver[pinKey(to)] = id;
    m_nextWireId = std::max(m_nextWireId, id + 1);
    ++m_topologyVersion;

    // Carry the driver's current level across immediately so a freshly wired
    // input is not left floating until the next evaluation.
    Component* source = component(from.component);
    Component* sink = component(to.component);
    sink->pin(to.pin).setSignal(source->pin(from.pin).signal());
    return id;
}

bool Circuit::disconnect(WireId id)
{
    auto it = std::find_if(m_wires.begin(), m_wires.end(),
                           [id](const Wire& w) { return w.id == id; });
    if (it == m_wires.end())
        return false;

    const PinRef sink = it->to;
    m_wires.erase(it);
    rebuildWireIndex();
    ++m_topologyVersion;
    // The pin may already be gone — shrinking a gate removes pins first and
    // then drops the wires that fed them.
    Component* c = component(sink.component);
    if (c && sink.pin >= 0 && static_cast<std::size_t>(sink.pin) < c->pinCount())
        c->pin(sink.pin).setSignal(Signal::Undefined);   // floating again
    return true;
}

std::vector<Wire> Circuit::wiresAt(ComponentId id) const
{
    std::vector<Wire> result;
    for (const Wire& w : m_wires) {
        if (w.from.component == id || w.to.component == id)
            result.push_back(w);
    }
    return result;
}

const Wire* Circuit::driverOf(PinRef input) const
{
    auto it = m_driver.find(pinKey(input));
    return it == m_driver.end() ? nullptr : wire(it->second);
}

const Wire* Circuit::wire(WireId id) const
{
    for (const Wire& w : m_wires) {
        if (w.id == id)
            return &w;
    }
    return nullptr;
}

const std::vector<PinRef>& Circuit::sinksOf(PinRef output) const
{
    static const std::vector<PinRef> kNoSinks;
    auto it = m_fanout.find(pinKey(output));
    return it == m_fanout.end() ? kNoSinks : it->second;
}

Signal Circuit::signalAt(PinRef ref) const
{
    Component* c = component(ref.component);
    if (!c || ref.pin < 0 || static_cast<std::size_t>(ref.pin) >= c->pinCount())
        return Signal::Undefined;
    return c->pin(ref.pin).signal();
}

std::vector<Wire> Circuit::setInputCount(ComponentId gateId, int inputs)
{
    Component* c = component(gateId);
    std::vector<Wire> removed;
    if (!c)
        return removed;

    const std::size_t before = c->pinCount();
    if (!c->setProperty("inputs", std::to_string(inputs)))
        return removed;
    const std::size_t after = c->pinCount();
    ++m_topologyVersion;
    if (after >= before)
        return removed;

    for (const Wire& w : m_wires) {
        if (w.to.component == gateId && static_cast<std::size_t>(w.to.pin) >= after)
            removed.push_back(w);
    }
    for (const Wire& w : removed)
        disconnect(w.id);
    return removed;
}

Circuit Circuit::extract(const std::vector<ComponentId>& ids) const
{
    Circuit fragment;
    fragment.setName(m_name);
    std::vector<ComponentId> kept;
    // Canvas order, not selection order, so a fragment always lists its parts
    // the same way whatever order they were clicked in.
    for (const std::unique_ptr<Component>& c : m_components) {
        if (std::find(ids.begin(), ids.end(), c->id()) == ids.end())
            continue;
        fragment.add(c->clone(), c->id());
        kept.push_back(c->id());
    }
    const auto inside = [&kept](ComponentId id) {
        return std::find(kept.begin(), kept.end(), id) != kept.end();
    };
    for (const Wire& w : m_wires) {
        if (inside(w.from.component) && inside(w.to.component))
            fragment.connect(w.from, w.to, w.id);
    }
    fragment.reset();          // a fragment carries shape, not signal levels
    return fragment;
}

FragmentIds Circuit::insert(const Circuit& fragment, double dx, double dy,
                            const FragmentIds& preferred)
{
    FragmentIds assigned;
    std::unordered_map<ComponentId, ComponentId> remap;

    std::size_t index = 0;
    for (const std::unique_ptr<Component>& c : fragment.components()) {
        std::unique_ptr<Component> copy = c->clone();
        copy->setPosition(c->x() + dx, c->y() + dy);
        const ComponentId want = index < preferred.components.size()
                                     ? preferred.components[index] : kNoComponent;
        // A fresh id, never the original's: the original may still be on the
        // canvas, and add() would reuse its id only if it were free anyway.
        const ComponentId id = add(std::move(copy), want != kNoComponent ? want : m_nextComponentId);
        remap[c->id()] = id;
        assigned.components.push_back(id);
        ++index;
    }

    index = 0;
    for (const Wire& w : fragment.wires()) {
        const WireId want = index < preferred.wires.size() ? preferred.wires[index] : kNoWire;
        const PinRef from{remap.at(w.from.component), w.from.pin};
        const PinRef to{remap.at(w.to.component), w.to.pin};
        assigned.wires.push_back(connect(from, to, want));
        ++index;
    }
    return assigned;
}

void Circuit::reset()
{
    for (const std::unique_ptr<Component>& c : m_components)
        c->reset();
}

std::vector<Component*> Circuit::componentsOfType(const std::string& typeName) const
{
    std::vector<Component*> result;
    for (const std::unique_ptr<Component>& c : m_components) {
        if (c->typeName() == typeName)
            result.push_back(c.get());
    }
    return result;
}

bool Circuit::hasSequentialParts() const
{
    return std::any_of(m_components.begin(), m_components.end(),
                       [](const std::unique_ptr<Component>& c) { return c->isSequential(); });
}

} // namespace lb
