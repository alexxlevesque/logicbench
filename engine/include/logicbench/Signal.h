#pragma once

#include <iosfwd>
#include <string>

namespace lb {

/// A wire in a real circuit is not a bool: before anything drives it, its value
/// is unknown. Modelling that third state is what lets LogicBench report a
/// floating input honestly instead of guessing Low, and what gives the
/// oscillation detector somewhere to put a signal it cannot resolve.
///
/// The operators below implement Kleene three-valued logic: Undefined
/// propagates unless the other operand already determines the result
/// (Low & X == Low, High | X == High).
enum class Signal : unsigned char {
    Low = 0,
    High = 1,
    Undefined = 2
};

constexpr Signal operator!(Signal a) noexcept
{
    switch (a) {
    case Signal::Low:  return Signal::High;
    case Signal::High: return Signal::Low;
    default:           return Signal::Undefined;
    }
}

constexpr Signal operator&(Signal a, Signal b) noexcept
{
    if (a == Signal::Low || b == Signal::Low)
        return Signal::Low;                       // a dominant zero settles it
    if (a == Signal::Undefined || b == Signal::Undefined)
        return Signal::Undefined;
    return Signal::High;
}

constexpr Signal operator|(Signal a, Signal b) noexcept
{
    if (a == Signal::High || b == Signal::High)
        return Signal::High;                      // a dominant one settles it
    if (a == Signal::Undefined || b == Signal::Undefined)
        return Signal::Undefined;
    return Signal::Low;
}

constexpr Signal operator^(Signal a, Signal b) noexcept
{
    if (a == Signal::Undefined || b == Signal::Undefined)
        return Signal::Undefined;                 // parity needs both operands
    return a == b ? Signal::Low : Signal::High;
}

constexpr Signal& operator&=(Signal& a, Signal b) noexcept { a = a & b; return a; }
constexpr Signal& operator|=(Signal& a, Signal b) noexcept { a = a | b; return a; }
constexpr Signal& operator^=(Signal& a, Signal b) noexcept { a = a ^ b; return a; }

constexpr Signal signalFromBool(bool b) noexcept
{
    return b ? Signal::High : Signal::Low;
}

/// '0', '1' or 'X' — the character used in truth tables, CSV export and the
/// JSON file format.
constexpr char toChar(Signal s) noexcept
{
    switch (s) {
    case Signal::Low:  return '0';
    case Signal::High: return '1';
    default:           return 'X';
    }
}

Signal signalFromChar(char c) noexcept;

std::string toString(Signal s);

std::ostream& operator<<(std::ostream& os, Signal s);

} // namespace lb
