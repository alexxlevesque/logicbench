#include "logicbench/Component.h"

namespace lb {

int Component::addPin(std::string name, PinDirection direction)
{
    m_pins.emplace_back(std::move(name), direction);
    return static_cast<int>(m_pins.size()) - 1;
}

bool Component::driveOutput(std::size_t index, Signal value)
{
    Pin& p = m_pins.at(index);
    if (p.signal() == value)
        return false;
    p.setSignal(value);
    return true;
}

int Component::pinIndex(const std::string& name) const
{
    for (std::size_t i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].name() == name)
            return static_cast<int>(i);
    }
    return -1;
}

std::vector<int> Component::inputPins() const
{
    std::vector<int> result;
    for (std::size_t i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].isInput())
            result.push_back(static_cast<int>(i));
    }
    return result;
}

std::vector<int> Component::outputPins() const
{
    std::vector<int> result;
    for (std::size_t i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].isOutput())
            result.push_back(static_cast<int>(i));
    }
    return result;
}

void Component::reset()
{
    for (Pin& p : m_pins)
        p.setSignal(Signal::Undefined);
}

bool Component::setProperty(const std::string& name, const std::string& value)
{
    if (name == "label") {
        m_label = value;
        return true;
    }
    if (name == "delay") {
        try {
            setDelay(std::stoi(value));
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }
    return false;
}

} // namespace lb
