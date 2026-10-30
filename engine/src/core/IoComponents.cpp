#include "logicbench/IoComponents.h"

namespace lb {

// ---------------------------------------------------------------- Switch ---

Switch::Switch()
{
    addPin("OUT", PinDirection::Output);
    setDelay(0);
}

bool Switch::evaluate()
{
    return driveOutput(0, m_state);
}

void Switch::reset()
{
    // The stored position survives a reset — resetting a circuit should not
    // silently flip the switches the user set.
    pin(0).setSignal(Signal::Undefined);
}

std::vector<PropertyDescriptor> Switch::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"state", "Closed", PropertyDescriptor::Kind::Boolean, 0, 1},
    };
}

std::map<std::string, std::string> Switch::properties() const
{
    return {
        {"label", label()},
        {"state", m_state == Signal::High ? "1" : "0"},
    };
}

bool Switch::setProperty(const std::string& name, const std::string& value)
{
    if (name == "state") {
        m_state = (value == "1" || value == "true") ? Signal::High : Signal::Low;
        return true;
    }
    return Component::setProperty(name, value);
}

// ------------------------------------------------------------------- Led ---

Led::Led()
{
    addPin("IN", PinDirection::Input);
    setDelay(0);
}

// ----------------------------------------------------------------- Clock ---

Clock::Clock()
{
    addPin("OUT", PinDirection::Output);
    setDelay(0);
}

bool Clock::flip()
{
    const Signal current = pin(0).signal();
    const Signal next = (current == Signal::High) ? Signal::Low : Signal::High;
    return driveOutput(0, next);
}

void Clock::reset()
{
    pin(0).setSignal(Signal::Low);
}

std::vector<PropertyDescriptor> Clock::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"period", "Period (ticks)", PropertyDescriptor::Kind::Integer, 2, 1024},
        {"running", "Running", PropertyDescriptor::Kind::Boolean, 0, 1},
    };
}

std::map<std::string, std::string> Clock::properties() const
{
    return {
        {"label", label()},
        {"period", std::to_string(m_period)},
        {"running", m_running ? "1" : "0"},
    };
}

bool Clock::setProperty(const std::string& name, const std::string& value)
{
    if (name == "period") {
        try {
            setPeriod(std::stoi(value));
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }
    if (name == "running") {
        m_running = (value == "1" || value == "true");
        return true;
    }
    return Component::setProperty(name, value);
}

} // namespace lb
