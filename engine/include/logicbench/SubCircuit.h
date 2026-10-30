#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"
#include "logicbench/Simulator.h"

#include <memory>
#include <string>
#include <vector>

namespace lb {

/// A whole circuit packaged as a single component — the Composite pattern.
/// A SubCircuit *is* a Component and *owns* a Circuit, so a full adder built
/// once drops into a 4-bit adder as one block, and blocks nest without the
/// rest of the system knowing.
///
/// Pins come from the inner circuit: every Switch becomes an input pin, every
/// LED an output pin, named by its label. That is why packaging needs no extra
/// port objects — you build a circuit the normal way and the switches and
/// lamps you already placed become the block's interface.
///
/// Copying is a deep copy of the inner circuit: two instances of the same
/// block on one canvas hold independent state, which matters the moment the
/// block contains a flip-flop.
class SubCircuit final : public Cloneable<SubCircuit, Component> {
public:
    SubCircuit();
    explicit SubCircuit(std::string blockName, std::unique_ptr<Circuit> inner);

    SubCircuit(const SubCircuit& other);
    ~SubCircuit() override = default;

    std::string typeName() const override { return "SUBCIRCUIT"; }
    bool evaluate() override;
    bool isSequential() const override;
    void reset() override;

    const std::string& blockName() const noexcept { return m_blockName; }
    void setBlockName(std::string name) { m_blockName = std::move(name); }

    Circuit& inner() { return *m_inner; }
    const Circuit& inner() const { return *m_inner; }

    /// Replaces the contents and recomputes the pin list.
    void setInner(std::unique_ptr<Circuit> inner);

    /// Rereads the inner switches and LEDs and rebuilds the pin list. Any wire
    /// attached to this block must be revalidated by the caller afterwards.
    void rebuildPins();

    /// Inner component backing a pin, or kNoComponent.
    ComponentId innerComponentForPin(std::size_t pinIndex) const;

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

private:
    static std::vector<Component*> sortedPorts(const Circuit& circuit,
                                               const std::string& typeName);

    std::string m_blockName = "block";
    std::unique_ptr<Circuit> m_inner;
    std::unique_ptr<Simulator> m_simulator;
    std::vector<ComponentId> m_pinToInner;   // parallel to the pin list
};

} // namespace lb
