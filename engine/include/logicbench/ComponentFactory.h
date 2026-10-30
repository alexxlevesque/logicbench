#pragma once

#include "logicbench/Component.h"
#include "logicbench/Errors.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lb {

/// What the palette needs to show one entry, kept in the engine so the GUI
/// holds no list of component types of its own. Adding a gate is then a
/// one-line registration, not an edit in three files.
struct ComponentDescriptor {
    std::string typeName;      ///< stable key used in files, e.g. "NAND"
    std::string displayName;   ///< palette text, e.g. "NAND gate"
    std::string category;      ///< palette grouping, e.g. "Gates"
    std::string summary;       ///< tooltip
};

/// Creates components by type name — for the palette, for paste, and for the
/// loader, which has nothing but a type string to work from.
class ComponentFactory {
public:
    using Creator = std::function<std::unique_ptr<Component>()>;

    void registerType(ComponentDescriptor descriptor, Creator creator);

    /// Throws UnknownComponentType if nothing is registered under that name.
    std::unique_ptr<Component> create(const std::string& typeName) const;

    bool isRegistered(const std::string& typeName) const;

    const std::vector<ComponentDescriptor>& descriptors() const noexcept
    {
        return m_descriptors;
    }

    /// Registered types in one category, in registration order.
    std::vector<ComponentDescriptor> inCategory(const std::string& category) const;

    std::vector<std::string> categories() const;

    /// The built-in library: gates, switch, LED, clock, D flip-flop and the
    /// empty subcircuit placeholder used when loading a file.
    static const ComponentFactory& standard();

private:
    std::vector<ComponentDescriptor> m_descriptors;
    std::vector<Creator> m_creators;
};

} // namespace lb
