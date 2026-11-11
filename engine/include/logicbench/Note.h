#pragma once

#include "logicbench/Cloneable.h"
#include "logicbench/Component.h"

#include <string>

namespace lb {

/// A text note on the canvas.
///
/// A note is a component with no pins rather than a separate kind of canvas
/// object, so it saves, copies, pastes, moves and undoes through the code that
/// already does all of that for components. It never takes part in the
/// simulation: with no pins it is never woken.
class Note final : public Cloneable<Note, Component> {
public:
    Note();

    std::string typeName() const override { return "NOTE"; }
    bool evaluate() override { return false; }

    const std::string& text() const noexcept { return m_text; }
    void setText(std::string text) { m_text = std::move(text); }

    std::vector<PropertyDescriptor> propertyDescriptors() const override;
    std::map<std::string, std::string> properties() const override;
    bool setProperty(const std::string& name, const std::string& value) override;

private:
    std::string m_text = "Note";
};

} // namespace lb
