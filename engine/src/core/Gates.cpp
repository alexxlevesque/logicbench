#include "logicbench/Gates.h"

#include <algorithm>
#include <string>

namespace lb {

namespace {
std::string inputName(int index)
{
    return std::string(1, static_cast<char>('A' + index));
}
} // namespace

Gate::Gate(int inputs, bool invertOutput, int minInputs, int maxInputs)
    : m_invert(invertOutput)
    , m_minInputs(minInputs)
    , m_maxInputs(std::max(minInputs, maxInputs))
    , m_inputCount(std::clamp(inputs, minInputs, std::max(minInputs, maxInputs)))
{
    buildPins();
}

void Gate::buildPins()
{
    clearPins();
    addPin("Y", PinDirection::Output);
    for (int i = 0; i < m_inputCount; ++i)
        addPin(inputName(i), PinDirection::Input);
}

void Gate::setInputCount(int n)
{
    const int clamped = std::clamp(n, m_minInputs, m_maxInputs);
    if (clamped == m_inputCount)
        return;
    m_inputCount = clamped;
    buildPins();
}

bool Gate::evaluate()
{
    Signal acc = pin(1).signal();
    for (int i = 1; i < m_inputCount; ++i)
        acc = op(acc, pin(static_cast<std::size_t>(i) + 1).signal());
    if (m_invert)
        acc = !acc;
    return driveOutput(0, acc);
}

std::vector<PropertyDescriptor> Gate::propertyDescriptors() const
{
    std::vector<PropertyDescriptor> descriptors;
    descriptors.push_back({"label", "Label", PropertyDescriptor::Kind::Text, 0, 0});
    if (m_minInputs < m_maxInputs) {
        descriptors.push_back({"inputs", "Inputs", PropertyDescriptor::Kind::Integer,
                               m_minInputs, m_maxInputs});
    }
    descriptors.push_back({"delay", "Delay (ticks)", PropertyDescriptor::Kind::Integer, 0, 64});
    return descriptors;
}

std::map<std::string, std::string> Gate::properties() const
{
    return {
        {"label", label()},
        {"inputs", std::to_string(m_inputCount)},
        {"delay", std::to_string(delay())},
    };
}

bool Gate::setProperty(const std::string& name, const std::string& value)
{
    if (name == "inputs") {
        try {
            setInputCount(std::stoi(value));
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }
    return Component::setProperty(name, value);
}

} // namespace lb
