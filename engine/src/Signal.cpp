#include "logicbench/Signal.h"

#include <ostream>

namespace lb {

Signal signalFromChar(char c) noexcept
{
    switch (c) {
    case '0': return Signal::Low;
    case '1': return Signal::High;
    default:  return Signal::Undefined;
    }
}

std::string toString(Signal s)
{
    return std::string(1, toChar(s));
}

std::ostream& operator<<(std::ostream& os, Signal s)
{
    return os << toChar(s);
}

} // namespace lb
