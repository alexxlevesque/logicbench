#include "logicbench/SubCircuit.h"

#include "logicbench/IoComponents.h"

#include <algorithm>

namespace lb {

std::vector<Component*> SubCircuit::sortedPorts(const Circuit& circuit,
                                                const std::string& typeName)
{
    std::vector<Component*> ports = circuit.componentsOfType(typeName);
    // Labelled ports come first in alphabetical order, unlabelled ones after,
    // so the pin order on the block is the order the user typed into the pin
    // editor and does not shuffle when the block is edited.
    std::sort(ports.begin(), ports.end(), [](const Component* a, const Component* b) {
        const bool aLabelled = !a->label().empty();
        const bool bLabelled = !b->label().empty();
        if (aLabelled != bLabelled)
            return aLabelled;
        if (aLabelled && a->label() != b->label())
            return a->label() < b->label();
        return a->id() < b->id();
    });
    return ports;
}

SubCircuit::SubCircuit()
    : m_inner(std::make_unique<Circuit>())
    , m_simulator(std::make_unique<Simulator>(*m_inner))
{
    setDelay(1);
}

SubCircuit::SubCircuit(std::string blockName, std::unique_ptr<Circuit> inner)
    : m_blockName(std::move(blockName))
    , m_inner(inner ? std::move(inner) : std::make_unique<Circuit>())
    , m_simulator(std::make_unique<Simulator>(*m_inner))
{
    setDelay(1);
    rebuildPins();
    m_simulator->reset();
}

SubCircuit::SubCircuit(const SubCircuit& other)
    : Cloneable<SubCircuit, Component>(other)
    , m_blockName(other.m_blockName)
    , m_inner(std::make_unique<Circuit>(*other.m_inner))   // deep copy, not a shared pointer
    , m_simulator(std::make_unique<Simulator>(*m_inner))
    , m_pinToInner(other.m_pinToInner)
{
    m_simulator->reset();
}

void SubCircuit::setInner(std::unique_ptr<Circuit> inner)
{
    m_inner = inner ? std::move(inner) : std::make_unique<Circuit>();
    m_simulator = std::make_unique<Simulator>(*m_inner);
    rebuildPins();
    m_simulator->reset();
}

void SubCircuit::rebuildPins()
{
    clearPins();
    m_pinToInner.clear();

    for (Component* port : sortedPorts(*m_inner, "SWITCH")) {
        addPin(port->label().empty() ? ("IN" + std::to_string(port->id())) : port->label(),
               PinDirection::Input);
        m_pinToInner.push_back(port->id());
    }
    for (Component* port : sortedPorts(*m_inner, "LED")) {
        addPin(port->label().empty() ? ("OUT" + std::to_string(port->id())) : port->label(),
               PinDirection::Output);
        m_pinToInner.push_back(port->id());
    }
}

ComponentId SubCircuit::innerComponentForPin(std::size_t pinIndex) const
{
    return pinIndex < m_pinToInner.size() ? m_pinToInner[pinIndex] : kNoComponent;
}

bool SubCircuit::evaluate()
{
    // Drive the inner switches from this block's input pins...
    for (std::size_t i = 0; i < pinCount(); ++i) {
        if (!pin(i).isInput())
            continue;
        Component* inner = m_inner->component(innerComponentForPin(i));
        if (auto* sw = dynamic_cast<Switch*>(inner)) {
            if (sw->state() != pin(i).signal()) {
                sw->setState(pin(i).signal());
                m_simulator->schedule(sw->id());
            }
        }
    }

    // ...let the inner circuit settle...
    m_simulator->settle();

    // ...and publish the inner lamps on this block's output pins.
    bool changed = false;
    for (std::size_t i = 0; i < pinCount(); ++i) {
        if (!pin(i).isOutput())
            continue;
        Component* inner = m_inner->component(innerComponentForPin(i));
        const Signal value = inner ? inner->pin(0).signal() : Signal::Undefined;
        changed |= driveOutput(i, value);
    }
    return changed;
}

bool SubCircuit::isSequential() const
{
    return m_inner->hasSequentialParts();
}

void SubCircuit::reset()
{
    m_simulator->reset();
    Component::reset();
}

std::vector<PropertyDescriptor> SubCircuit::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"block", "Block name", PropertyDescriptor::Kind::Text, 0, 0},
        {"delay", "Delay (ticks)", PropertyDescriptor::Kind::Integer, 0, 64},
    };
}

std::map<std::string, std::string> SubCircuit::properties() const
{
    return {
        {"label", label()},
        {"block", m_blockName},
        {"delay", std::to_string(delay())},
    };
}

bool SubCircuit::setProperty(const std::string& name, const std::string& value)
{
    if (name == "block") {
        m_blockName = value;
        return true;
    }
    return Component::setProperty(name, value);
}

} // namespace lb
