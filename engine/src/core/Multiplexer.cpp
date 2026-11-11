#include "logicbench/Multiplexer.h"

#include <vector>

namespace lb {

Multiplexer::Multiplexer(int dataInputs)
    : m_dataInputs(dataInputs == 4 ? 4 : 2)
{
    addPin("Y", PinDirection::Output);
    for (int i = 0; i < m_dataInputs; ++i)
        addPin("D" + std::to_string(i), PinDirection::Input);
    for (int i = 0; i < selectCount(); ++i)
        addPin("S" + std::to_string(i), PinDirection::Input);
    setDelay(1);
}

bool Multiplexer::evaluate()
{
    // Every data input the select lines could be pointing at: an undefined
    // select bit keeps both branches open.
    std::vector<int> candidates{0};
    for (int bit = 0; bit < selectCount(); ++bit) {
        const Signal s = pin(static_cast<std::size_t>(1 + m_dataInputs + bit)).signal();
        std::vector<int> next;
        for (int index : candidates) {
            if (s != Signal::High)
                next.push_back(index);                 // bit could be 0
            if (s != Signal::Low)
                next.push_back(index | (1 << bit));    // bit could be 1
        }
        candidates = next;
    }

    Signal out = pin(static_cast<std::size_t>(1 + candidates.front())).signal();
    for (int index : candidates) {
        if (pin(static_cast<std::size_t>(1 + index)).signal() != out) {
            out = Signal::Undefined;
            break;
        }
    }
    return driveOutput(0, out);
}

std::vector<PropertyDescriptor> Multiplexer::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"delay", "Delay (ticks)", PropertyDescriptor::Kind::Integer, 0, 64},
    };
}

std::map<std::string, std::string> Multiplexer::properties() const
{
    return {
        {"label", label()},
        {"delay", std::to_string(delay())},
    };
}

} // namespace lb
