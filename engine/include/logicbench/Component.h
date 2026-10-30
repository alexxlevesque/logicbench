#pragma once

#include "logicbench/Pin.h"
#include "logicbench/Signal.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lb {

/// How the properties dock should present one editable property. The engine
/// publishes this so the GUI can build an editor for any component type
/// without knowing what that type is.
struct PropertyDescriptor {
    enum class Kind { Integer, Boolean, Text };

    std::string name;
    std::string label;
    Kind kind = Kind::Text;
    int minimum = 0;
    int maximum = 0;
};

/// Base class of everything that can sit on the canvas.
///
/// A component owns its pins, evaluates outputs from inputs, and can clone
/// itself. clone() is a virtual copy constructor: the palette holds one
/// prototype of each type, copy-paste duplicates a selection, and a SubCircuit
/// must deep-copy the whole circuit it contains.
class Component {
public:
    virtual ~Component() = default;

    Component(const Component&) = default;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

    virtual std::string typeName() const = 0;
    virtual std::unique_ptr<Component> clone() const = 0;

    /// Recompute output pins from input pins.
    /// Returns true if any output pin changed value, which is the signal the
    /// simulator uses to decide whether to wake this component's readers.
    virtual bool evaluate() = 0;

    /// Sequential parts hold state between evaluations, so a truth table over
    /// them is not well defined. TruthTableGenerator refuses on this basis.
    virtual bool isSequential() const { return false; }

    /// Clears stored state and drives every pin back to Undefined.
    virtual void reset();

    virtual std::vector<PropertyDescriptor> propertyDescriptors() const { return {}; }
    virtual std::map<std::string, std::string> properties() const { return {}; }
    virtual bool setProperty(const std::string& name, const std::string& value);

    ComponentId id() const noexcept { return m_id; }
    void setId(ComponentId id) noexcept { m_id = id; }

    const std::string& label() const noexcept { return m_label; }
    void setLabel(std::string label) { m_label = std::move(label); }

    /// Propagation delay in simulator ticks. Zero-delay parts are legal and
    /// evaluate in the same tick.
    int delay() const noexcept { return m_delay; }
    void setDelay(int ticks) noexcept { m_delay = ticks < 0 ? 0 : ticks; }

    double x() const noexcept { return m_x; }
    double y() const noexcept { return m_y; }
    void setPosition(double x, double y) noexcept { m_x = x; m_y = y; }

    std::size_t pinCount() const noexcept { return m_pins.size(); }
    Pin& pin(std::size_t i) { return m_pins.at(i); }
    const Pin& pin(std::size_t i) const { return m_pins.at(i); }

    /// Index of the first pin with this name, or -1.
    int pinIndex(const std::string& name) const;

    std::vector<int> inputPins() const;
    std::vector<int> outputPins() const;

protected:
    Component() = default;

    int addPin(std::string name, PinDirection direction);
    void clearPins() { m_pins.clear(); }

    /// Writes an output pin and reports whether the value actually changed.
    bool driveOutput(std::size_t index, Signal value);

private:
    ComponentId m_id = kNoComponent;
    std::string m_label;
    int m_delay = 1;
    double m_x = 0.0;
    double m_y = 0.0;
    std::vector<Pin> m_pins;
};

} // namespace lb
