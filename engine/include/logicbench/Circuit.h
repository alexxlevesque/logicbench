#pragma once

#include "logicbench/Component.h"
#include "logicbench/Errors.h"
#include "logicbench/Pin.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lb {

/// The ids given to the parts of an inserted fragment, in the fragment's own
/// order. Undo removes exactly these; redo passes them back as the preferred
/// ids, so a pasted circuit comes back under the same ids after undo and redo.
struct FragmentIds {
    std::vector<ComponentId> components;
    std::vector<WireId> wires;

    bool empty() const noexcept { return components.empty(); }
};

/// Owns every component on one canvas and every wire between them.
///
/// Ownership is through unique_ptr and nothing else holds an owning pointer,
/// so deleting a component cannot leave a live reference behind. Wires refer
/// to pins by {id, index} rather than by pointer, which is what makes undo and
/// the file format straightforward: an id is still valid after the object it
/// names has been destroyed and rebuilt.
///
/// The copy constructor and copy assignment are a deep copy: every component
/// is cloned and the wire list is carried over verbatim (ids are preserved by
/// the clone, so the wires still resolve). SubCircuit relies on this.
class Circuit {
public:
    Circuit() = default;
    ~Circuit() = default;

    Circuit(const Circuit& other);
    Circuit& operator=(const Circuit& other);
    Circuit(Circuit&&) noexcept = default;
    Circuit& operator=(Circuit&&) noexcept = default;

    const std::string& name() const noexcept { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    /// Takes ownership and returns the id assigned. If preferredId is free it
    /// is used, which is how undo restores a component under its old id and
    /// keeps the wires that referenced it valid.
    ComponentId add(std::unique_ptr<Component> component,
                    ComponentId preferredId = kNoComponent);

    /// Removes the component and every wire touching it. Returns the component
    /// so a command can put it back; returns nullptr if the id is unknown.
    std::unique_ptr<Component> take(ComponentId id);

    bool remove(ComponentId id) { return take(id) != nullptr; }

    Component* component(ComponentId id) const;
    bool contains(ComponentId id) const { return component(id) != nullptr; }

    const std::vector<std::unique_ptr<Component>>& components() const noexcept
    {
        return m_components;
    }
    std::size_t componentCount() const noexcept { return m_components.size(); }

    /// Connects an output pin to an input pin.
    /// Throws IllegalConnection for a reversed, duplicated or already-driven
    /// connection. Returns the new wire's id.
    WireId connect(PinRef from, PinRef to, WireId preferredId = kNoWire);

    bool disconnect(WireId id);

    const std::vector<Wire>& wires() const noexcept { return m_wires; }

    /// Wires with either end on this component.
    std::vector<Wire> wiresAt(ComponentId id) const;

    /// The wire driving this input pin, or nullptr if it is floating.
    const Wire* driverOf(PinRef input) const;

    const Wire* wire(WireId id) const;

    /// Every input pin fed by this output pin.
    const std::vector<PinRef>& sinksOf(PinRef output) const;

    Signal signalAt(PinRef ref) const;

    /// Changes a gate's input count, removing wires into any pin that
    /// disappears. Returns the wires that were removed so a command can
    /// restore them on undo.
    std::vector<Wire> setInputCount(ComponentId gate, int inputs);

    /// A new circuit holding copies of these components and of every wire with
    /// both ends among them; wires to anything outside are dropped. Ids and
    /// positions are kept, so the fragment can be inserted somewhere else.
    /// Unknown ids are ignored.
    Circuit extract(const std::vector<ComponentId>& ids) const;

    /// Adds a copy of every component and wire of `fragment`, moved by (dx, dy).
    /// Each part gets a fresh id unless `preferred` names a free one for it (by
    /// position in the fragment), which is how redo restores a paste exactly.
    /// Returns the ids assigned, in the fragment's order.
    FragmentIds insert(const Circuit& fragment, double dx = 0.0, double dy = 0.0,
                       const FragmentIds& preferred = {});

    /// Clears all stored state and drives every pin Undefined.
    void reset();

    /// Components of one type, in insertion order.
    std::vector<Component*> componentsOfType(const std::string& typeName) const;

    /// True if any component in the circuit (or in a nested subcircuit) holds
    /// state between evaluations.
    bool hasSequentialParts() const;

    /// Bumped by every structural edit. Analyses that depend on the shape of
    /// the circuit rather than on its signal values cache against this.
    std::uint64_t topologyVersion() const noexcept { return m_topologyVersion; }

    ComponentId nextComponentId() const noexcept { return m_nextComponentId; }
    WireId nextWireId() const noexcept { return m_nextWireId; }

private:
    void reindex();
    void rebuildWireIndex();
    void validateEndpoints(PinRef from, PinRef to) const;

    static std::uint64_t pinKey(PinRef ref) noexcept
    {
        return (static_cast<std::uint64_t>(ref.component) << 32)
             | static_cast<std::uint32_t>(ref.pin);
    }

    std::string m_name = "untitled";
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_map<ComponentId, Component*> m_index;
    std::vector<Wire> m_wires;
    // Derived from m_wires and rebuilt whenever it changes. The simulator
    // walks fan-out once per changed output pin, so that lookup has to be a
    // hash probe rather than a scan of every wire in the circuit.
    std::unordered_map<std::uint64_t, std::vector<PinRef>> m_fanout;
    std::unordered_map<std::uint64_t, WireId> m_driver;
    ComponentId m_nextComponentId = 1;
    WireId m_nextWireId = 1;
    std::uint64_t m_topologyVersion = 1;
};

} // namespace lb
