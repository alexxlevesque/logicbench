#include "logicbench/Decoder.h"

namespace lb {

Decoder::Decoder(int addressBits)
    : m_addressBits(addressBits == 3 ? 3 : 2)
{
    for (int i = 0; i < outputCount(); ++i)
        addPin("Y" + std::to_string(i), PinDirection::Output);
    for (int i = 0; i < m_addressBits; ++i)
        addPin("A" + std::to_string(i), PinDirection::Input);
    setDelay(1);
}

bool Decoder::evaluate()
{
    bool changed = false;
    for (int out = 0; out < outputCount(); ++out) {
        // Output `out` is the AND of one literal per address bit: A_i where
        // bit i of `out` is 1, A_i' where it is 0. Kleene AND gives the X rules.
        Signal value = Signal::High;
        for (int bit = 0; bit < m_addressBits; ++bit) {
            const Signal a = pin(static_cast<std::size_t>(outputCount() + bit)).signal();
            value = value & (((out >> bit) & 1) ? a : !a);
        }
        changed |= driveOutput(static_cast<std::size_t>(out), value);
    }
    return changed;
}

std::vector<PropertyDescriptor> Decoder::propertyDescriptors() const
{
    return {
        {"label", "Label", PropertyDescriptor::Kind::Text, 0, 0},
        {"delay", "Delay (ticks)", PropertyDescriptor::Kind::Integer, 0, 64},
    };
}

std::map<std::string, std::string> Decoder::properties() const
{
    return {
        {"label", label()},
        {"delay", std::to_string(delay())},
    };
}

} // namespace lb
