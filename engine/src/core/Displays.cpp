#include "logicbench/Displays.h"

namespace lb {

namespace {

std::vector<PropertyDescriptor> labelOnly()
{
    return {{"label", "Label", PropertyDescriptor::Kind::Text, 0, 0}};
}

} // namespace

// ------------------------------------------------------------ HexDisplay ---

HexDisplay::HexDisplay()
{
    for (int i = 0; i < 4; ++i)
        addPin("D" + std::to_string(i), PinDirection::Input);
    setDelay(0);
}

int HexDisplay::digit() const
{
    int value = 0;
    for (int i = 0; i < 4; ++i) {
        const Signal s = pin(static_cast<std::size_t>(i)).signal();
        if (s == Signal::Undefined)
            return -1;
        if (s == Signal::High)
            value |= 1 << i;
    }
    return value;
}

std::vector<PropertyDescriptor> HexDisplay::propertyDescriptors() const { return labelOnly(); }

std::map<std::string, std::string> HexDisplay::properties() const
{
    return {{"label", label()}};
}

// --------------------------------------------------- SevenSegmentDisplay ---

SevenSegmentDisplay::SevenSegmentDisplay()
{
    for (char name = 'a'; name < 'a' + kSegments; ++name)
        addPin(std::string(1, name), PinDirection::Input);
    setDelay(0);
}

Signal SevenSegmentDisplay::segment(int i) const
{
    if (i < 0 || i >= kSegments)
        return Signal::Undefined;
    return pin(static_cast<std::size_t>(i)).signal();
}

std::vector<PropertyDescriptor> SevenSegmentDisplay::propertyDescriptors() const
{
    return labelOnly();
}

std::map<std::string, std::string> SevenSegmentDisplay::properties() const
{
    return {{"label", label()}};
}

} // namespace lb
