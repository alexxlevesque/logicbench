#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

#include <string>

namespace lb {

/// A 2-to-4 or 3-to-8 decoder: exactly one output is high, the one whose
/// number the address inputs spell in binary.
///
/// Pins: outputs Y0 .. Y(2^k - 1) first, then the address inputs A0 .. A(k-1),
/// A0 the least significant bit. Registered as DEC2 and DEC3 so a file names
/// the size before any wire is attached.
///
/// With an unknown address bit, an output is still a definite 0 when a known
/// bit already rules it out, and X only when it might be the chosen one.
class Decoder final : public Cloneable<Decoder, Component> {
public:
    /// addressBits is 2 or 3; anything else is treated as 2.
    explicit Decoder(int addressBits = 2);

    std::string typeName() const override { return m_addressBits == 3 ? "DEC3" : "DEC2"; }
    bool evaluate() override;

    int addressBits() const noexcept { return m_addressBits; }
    int outputCount() const noexcept { return 1 << m_addressBits; }

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;

private:
    int m_addressBits;
};

} // namespace lb
