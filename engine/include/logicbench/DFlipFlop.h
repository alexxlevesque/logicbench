#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

namespace lb {

/// Positive-edge-triggered D flip-flop.
///
/// This is the class that makes the simulator sequential, and the reason a
/// feedback path is not automatically an oscillation: Q changes only on a
/// rising clock edge, so wiring Q-bar back to D gives a toggle rather than a
/// runaway loop.
///
/// Pins: 0 = Q (out), 1 = QBAR (out), 2 = D (in), 3 = CLK (in).
class DFlipFlop final : public Cloneable<DFlipFlop, Component> {
public:
    DFlipFlop();

    std::string typeName() const override { return "DFF"; }
    bool evaluate() override;
    bool isSequential() const override { return true; }
    void reset() override;

    Signal state() const noexcept { return m_state; }

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

private:
    Signal m_state = Signal::Low;
    Signal m_lastClock = Signal::Undefined;
};

} // namespace lb
