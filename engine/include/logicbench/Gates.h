#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

namespace lb {

/// Every logic gate is the same machine: fold a binary operation across the
/// input pins, optionally invert, drive the output. Only op() differs, which
/// is why NAND is AND plus a bubble rather than a separate truth table.
///
/// Pin 0 is always the output Y; inputs are pins 1..n named A, B, C...
/// Putting the output first keeps its index stable when the input count
/// changes, so existing wires out of the gate survive the edit.
class Gate : public Component {
public:
    static constexpr int kMaxInputs = 8;

    int inputCount() const noexcept { return m_inputCount; }

    /// Clamped to this gate's legal range. Shrinking drops the trailing input
    /// pins; Circuit::setInputCount removes any wires that fed them.
    void setInputCount(int n);

    bool evaluate() override;

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

    /// True for NAND, NOR, NOT — the shapes the editor draws with a bubble.
    bool isInverting() const noexcept { return m_invert; }

protected:
    Gate(int inputs, bool invertOutput, int minInputs = 2, int maxInputs = kMaxInputs);

    /// The gate's binary operation, folded left across the inputs.
    virtual Signal op(Signal a, Signal b) const = 0;

private:
    void buildPins();

    bool m_invert;
    int m_minInputs;
    int m_maxInputs;
    int m_inputCount;
};

class AndGate final : public Cloneable<AndGate, Gate> {
public:
    AndGate() : Cloneable(2, false) {}
    std::string typeName() const override { return "AND"; }
protected:
    Signal op(Signal a, Signal b) const override { return a & b; }
};

class OrGate final : public Cloneable<OrGate, Gate> {
public:
    OrGate() : Cloneable(2, false) {}
    std::string typeName() const override { return "OR"; }
protected:
    Signal op(Signal a, Signal b) const override { return a | b; }
};

class XorGate final : public Cloneable<XorGate, Gate> {
public:
    XorGate() : Cloneable(2, false) {}
    std::string typeName() const override { return "XOR"; }
protected:
    Signal op(Signal a, Signal b) const override { return a ^ b; }
};

class NandGate final : public Cloneable<NandGate, Gate> {
public:
    NandGate() : Cloneable(2, true) {}
    std::string typeName() const override { return "NAND"; }
protected:
    Signal op(Signal a, Signal b) const override { return a & b; }
};

class NorGate final : public Cloneable<NorGate, Gate> {
public:
    NorGate() : Cloneable(2, true) {}
    std::string typeName() const override { return "NOR"; }
protected:
    Signal op(Signal a, Signal b) const override { return a | b; }
};

/// One input, inverted. The fold never runs, so op() is the identity.
class NotGate final : public Cloneable<NotGate, Gate> {
public:
    // Exactly one input: an inverter with two inputs is a different gate.
    NotGate() : Cloneable(1, true, 1, 1) {}
    std::string typeName() const override { return "NOT"; }
protected:
    Signal op(Signal a, Signal) const override { return a; }
};

} // namespace lb
