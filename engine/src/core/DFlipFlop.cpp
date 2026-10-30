#include "logicbench/DFlipFlop.h"

namespace lb {

DFlipFlop::DFlipFlop()
{
    addPin("Q", PinDirection::Output);
    addPin("QBAR", PinDirection::Output);
    addPin("D", PinDirection::Input);
    addPin("CLK", PinDirection::Input);
    setDelay(1);
}

bool DFlipFlop::evaluate()
{
    const Signal clock = pin(3).signal();
    const bool rising = (m_lastClock == Signal::Low && clock == Signal::High);
    m_lastClock = clock;

    if (rising)
        m_state = pin(2).signal();          // latch D on the edge, X included

    bool changed = driveOutput(0, m_state);
    changed |= driveOutput(1, !m_state);
    return changed;
}

void DFlipFlop::reset()
{
    m_state = Signal::Low;
    m_lastClock = Signal::Undefined;
    Component::reset();
}

std::vector<PropertyDescriptor> DFlipFlop::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"initial", "Initial Q", PropertyDescriptor::Kind::Boolean, 0, 1},
        {"delay", "Delay (ticks)", PropertyDescriptor::Kind::Integer, 0, 64},
    };
}

std::map<std::string, std::string> DFlipFlop::properties() const
{
    return {
        {"label", label()},
        {"initial", m_state == Signal::High ? "1" : "0"},
        {"delay", std::to_string(delay())},
    };
}

bool DFlipFlop::setProperty(const std::string& name, const std::string& value)
{
    if (name == "initial") {
        m_state = (value == "1" || value == "true") ? Signal::High : Signal::Low;
        return true;
    }
    return Component::setProperty(name, value);
}

} // namespace lb
