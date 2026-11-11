#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

#include <string>

namespace lb {

/// Shows the hexadecimal digit (0 to F) on its four inputs.
///
/// Pins: D0 .. D3, all inputs, D0 the least significant bit. A display drives
/// nothing, so evaluate() never reports a change; the canvas reads digit().
class HexDisplay final : public Cloneable<HexDisplay, Component> {
public:
    HexDisplay();

    std::string typeName() const override { return "HEX"; }
    bool evaluate() override { return false; }

    /// 0 to 15, or -1 when any input is X (the display shows a dash).
    int digit() const;

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
};

/// Seven segments lit directly by their inputs, a to g in the usual order
/// (a top, then clockwise, g the middle bar).
///
/// Pins: a, b, c, d, e, f, g, all inputs.
class SevenSegmentDisplay final : public Cloneable<SevenSegmentDisplay, Component> {
public:
    static constexpr int kSegments = 7;

    SevenSegmentDisplay();

    std::string typeName() const override { return "SEG7"; }
    bool evaluate() override { return false; }

    /// The signal on segment i (0 = a .. 6 = g). X is drawn as a dimmed outline.
    Signal segment(int i) const;

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
};

} // namespace lb
