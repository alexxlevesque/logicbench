#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

namespace lb {

/// A switch the user flips. Switches are also what a circuit exposes as input
/// pins when it is packaged into a SubCircuit, and what TruthTableGenerator
/// enumerates over.
class Switch final : public Cloneable<Switch, Component> {
public:
    Switch();

    std::string typeName() const override { return "SWITCH"; }
    bool evaluate() override;
    void reset() override;

    Signal state() const noexcept { return m_state; }
    void setState(Signal s) noexcept { m_state = s; }
    void toggle() noexcept { m_state = (m_state == Signal::High) ? Signal::Low : Signal::High; }

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

private:
    Signal m_state = Signal::Low;
};

/// An output lamp. LEDs are a circuit's output pins once it is packaged.
class Led final : public Cloneable<Led, Component> {
public:
    Led();

    std::string typeName() const override { return "LED"; }
    bool evaluate() override { return false; }   // nothing downstream to drive

    Signal value() const { return pin(0).signal(); }
};

/// A free-running clock. The component itself holds no logic: Simulator
/// toggles it on period boundaries, which keeps all time in one place.
class Clock final : public Cloneable<Clock, Component> {
public:
    Clock();

    std::string typeName() const override { return "CLOCK"; }
    bool evaluate() override { return false; }
    void reset() override;

    /// Full period in ticks; the output is high for half of it.
    int period() const noexcept { return m_period; }
    void setPeriod(int ticks) noexcept { m_period = ticks < 2 ? 2 : ticks; }

    bool running() const noexcept { return m_running; }
    void setRunning(bool running) noexcept { m_running = running; }

    /// Flips the output pin. Returns true if the value changed.
    bool flip();

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

private:
    int m_period = 8;
    bool m_running = true;
};

} // namespace lb
