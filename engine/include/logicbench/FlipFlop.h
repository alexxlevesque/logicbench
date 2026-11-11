#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

#include <string>
#include <vector>

namespace lb {

/// Base of every positive-edge-triggered flip-flop.
///
/// The edge detection, the stored state and the two outputs are the same for
/// every kind, so they are written once here (Template Method): evaluate()
/// watches the clock and, on a rising edge only, asks the subclass for the next
/// state. A subclass supplies nextState() and the names of its data inputs.
///
/// Pins: 0 = Q (out), 1 = QBAR (out), then the data inputs in the order given,
/// then CLK. Keeping Q and QBAR first means a file written before this class
/// existed still wires a D flip-flop to the same pin numbers.
class FlipFlop : public Component {
public:
    bool evaluate() override;
    bool isSequential() const override { return true; }
    void reset() override;

    Signal state() const noexcept { return m_state; }

    /// Index of the clock pin (always the last pin).
    int clockPin() const noexcept { return static_cast<int>(pinCount()) - 1; }

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

protected:
    explicit FlipFlop(const std::vector<std::string>& dataInputs);

    /// The state to latch on a rising clock edge, from the data inputs and the
    /// current state. Kleene logic, so an undefined input gives an undefined
    /// state unless the other operands decide it.
    virtual Signal nextState() const = 0;

    /// Signal on the i-th data input (0-based, after Q and QBAR).
    Signal dataInput(std::size_t i) const { return pin(2 + i).signal(); }

private:
    Signal m_state = Signal::Low;
    Signal m_initial = Signal::Low;
    Signal m_lastClock = Signal::Undefined;
};

/// D flip-flop: Q takes D on the rising edge.
/// Pins: 0 = Q, 1 = QBAR, 2 = D, 3 = CLK.
class DFlipFlop final : public Cloneable<DFlipFlop, FlipFlop> {
public:
    DFlipFlop() : Cloneable(std::vector<std::string>{"D"}) {}
    std::string typeName() const override { return "DFF"; }

protected:
    Signal nextState() const override { return dataInput(0); }
};

/// JK flip-flop: J sets, K resets, both together toggle, neither holds.
/// Pins: 0 = Q, 1 = QBAR, 2 = J, 3 = K, 4 = CLK.
class JKFlipFlop final : public Cloneable<JKFlipFlop, FlipFlop> {
public:
    JKFlipFlop() : Cloneable(std::vector<std::string>{"J", "K"}) {}
    std::string typeName() const override { return "JKFF"; }

protected:
    /// The characteristic equation Q+ = J·Q' + K'·Q.
    Signal nextState() const override
    {
        return (dataInput(0) & !state()) | (!dataInput(1) & state());
    }
};

/// T flip-flop: toggles when T is high, holds when it is low.
/// Pins: 0 = Q, 1 = QBAR, 2 = T, 3 = CLK.
class TFlipFlop final : public Cloneable<TFlipFlop, FlipFlop> {
public:
    TFlipFlop() : Cloneable(std::vector<std::string>{"T"}) {}
    std::string typeName() const override { return "TFF"; }

protected:
    /// Q+ = T xor Q.
    Signal nextState() const override { return dataInput(0) ^ state(); }
};

} // namespace lb
