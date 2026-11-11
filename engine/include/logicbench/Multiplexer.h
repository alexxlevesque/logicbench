#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

#include <string>

namespace lb {

/// A 2:1 or 4:1 multiplexer: the select lines, read as a binary number, choose
/// which data input reaches the output.
///
/// Pins: 0 = Y (out), then D0 .. D(n-1), then S0 .. S(k-1), S0 the least
/// significant select bit. The two sizes are registered as separate types
/// (MUX2 and MUX4) so a file names the size and the pin list is fixed before
/// any wire is attached.
///
/// An undefined select line does not make the output undefined by itself: if
/// every data input it could be choosing carries the same value, that value
/// is the output, as it would be on a real part.
class Multiplexer final : public Cloneable<Multiplexer, Component> {
public:
    /// dataInputs is 2 or 4; anything else is treated as 2.
    explicit Multiplexer(int dataInputs = 2);

    std::string typeName() const override { return m_dataInputs == 4 ? "MUX4" : "MUX2"; }
    bool evaluate() override;

    int dataInputCount() const noexcept { return m_dataInputs; }
    int selectCount() const noexcept { return m_dataInputs == 4 ? 2 : 1; }

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;

private:
    int m_dataInputs;
};

} // namespace lb
