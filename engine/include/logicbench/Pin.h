#pragma once

#include "logicbench/Signal.h"

#include <cstdint>
#include <string>

namespace lb {

class Component;

using ComponentId = std::uint32_t;
using WireId      = std::uint32_t;

inline constexpr ComponentId kNoComponent = 0;
inline constexpr WireId      kNoWire      = 0;

enum class PinDirection { Input, Output };

/// A pin knows its owner, its direction and the signal currently on it.
/// Input pins start Undefined, which is how a floating input reports itself
/// all the way up to the LED that displays an X.
class Pin {
public:
    Pin(std::string name, PinDirection direction)
        : m_name(std::move(name)), m_direction(direction) {}

    const std::string& name() const noexcept { return m_name; }
    PinDirection direction() const noexcept  { return m_direction; }
    bool isInput() const noexcept  { return m_direction == PinDirection::Input; }
    bool isOutput() const noexcept { return m_direction == PinDirection::Output; }

    Signal signal() const noexcept { return m_signal; }
    void setSignal(Signal s) noexcept { m_signal = s; }

private:
    std::string m_name;
    PinDirection m_direction;
    Signal m_signal = Signal::Undefined;
};

/// Identifies one pin of one component. Circuits refer to pins this way rather
/// than by raw pointer so that ids survive save/load and undo.
struct PinRef {
    ComponentId component = kNoComponent;
    int pin = -1;

    bool isValid() const noexcept { return component != kNoComponent && pin >= 0; }

    friend bool operator==(const PinRef& a, const PinRef& b) noexcept
    {
        return a.component == b.component && a.pin == b.pin;
    }
    friend bool operator!=(const PinRef& a, const PinRef& b) noexcept { return !(a == b); }
    friend bool operator<(const PinRef& a, const PinRef& b) noexcept
    {
        return a.component != b.component ? a.component < b.component : a.pin < b.pin;
    }
};

/// One output pin driving one input pin. An output may drive many wires; an
/// input may be driven by at most one, which is the rule Circuit::connect
/// enforces.
struct Wire {
    WireId id = kNoWire;
    PinRef from;   // output pin
    PinRef to;     // input pin
};

} // namespace lb
