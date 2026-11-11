#include "logicbench/FlipFlop.h"

namespace lb {

FlipFlop::FlipFlop(const std::vector<std::string>& dataInputs)
{
    addPin("Q", PinDirection::Output);
    addPin("QBAR", PinDirection::Output);
    for (const std::string& name : dataInputs)
        addPin(name, PinDirection::Input);
    addPin("CLK", PinDirection::Input);
    setDelay(1);
}

bool FlipFlop::evaluate()
{
    const Signal clock = pin(static_cast<std::size_t>(clockPin())).signal();
    const bool rising = (m_lastClock == Signal::Low && clock == Signal::High);
    m_lastClock = clock;

    if (rising)
        m_state = nextState();              // latch on the edge, X included

    bool changed = driveOutput(0, m_state);
    changed |= driveOutput(1, !m_state);
    return changed;
}

void FlipFlop::reset()
{
    m_state = m_initial;
    m_lastClock = Signal::Undefined;
    Component::reset();
}

std::vector<PropertyDescriptor> FlipFlop::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"initial", "Initial Q", PropertyDescriptor::Kind::Boolean, 0, 1},
        {"delay", "Delay (ticks)", PropertyDescriptor::Kind::Integer, 0, 64},
    };
}

std::map<std::string, std::string> FlipFlop::properties() const
{
    return {
        {"label", label()},
        {"initial", m_initial == Signal::High ? "1" : "0"},
        {"delay", std::to_string(delay())},
    };
}

bool FlipFlop::setProperty(const std::string& name, const std::string& value)
{
    if (name == "initial") {
        m_initial = (value == "1" || value == "true") ? Signal::High : Signal::Low;
        m_state = m_initial;
        return true;
    }
    return Component::setProperty(name, value);
}

} // namespace lb
