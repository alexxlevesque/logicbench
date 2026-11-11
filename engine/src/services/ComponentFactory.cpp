#include "logicbench/ComponentFactory.h"

#include "logicbench/FlipFlop.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Multiplexer.h"
#include "logicbench/SubCircuit.h"

#include <algorithm>

namespace lb {

void ComponentFactory::registerType(ComponentDescriptor descriptor, Creator creator)
{
    for (std::size_t i = 0; i < m_descriptors.size(); ++i) {
        if (m_descriptors[i].typeName == descriptor.typeName) {
            m_descriptors[i] = std::move(descriptor);
            m_creators[i] = std::move(creator);
            return;
        }
    }
    m_descriptors.push_back(std::move(descriptor));
    m_creators.push_back(std::move(creator));
}

std::unique_ptr<Component> ComponentFactory::create(const std::string& typeName) const
{
    for (std::size_t i = 0; i < m_descriptors.size(); ++i) {
        if (m_descriptors[i].typeName == typeName)
            return m_creators[i]();
    }
    throw UnknownComponentType(typeName);
}

bool ComponentFactory::isRegistered(const std::string& typeName) const
{
    return std::any_of(m_descriptors.begin(), m_descriptors.end(),
                       [&typeName](const ComponentDescriptor& d) {
                           return d.typeName == typeName;
                       });
}

std::vector<ComponentDescriptor> ComponentFactory::inCategory(const std::string& category) const
{
    std::vector<ComponentDescriptor> result;
    for (const ComponentDescriptor& d : m_descriptors) {
        if (d.category == category)
            result.push_back(d);
    }
    return result;
}

std::vector<std::string> ComponentFactory::categories() const
{
    std::vector<std::string> result;
    for (const ComponentDescriptor& d : m_descriptors) {
        if (std::find(result.begin(), result.end(), d.category) == result.end())
            result.push_back(d.category);
    }
    return result;
}

const ComponentFactory& ComponentFactory::standard()
{
    static const ComponentFactory registry = [] {
        ComponentFactory f;
        const auto gate = [&f](const char* type, const char* name, const char* summary,
                               ComponentFactory::Creator creator) {
            f.registerType({type, name, "Gates", summary}, std::move(creator));
        };
        gate("AND",  "AND",  "Output high when every input is high",
             [] { return std::make_unique<AndGate>(); });
        gate("OR",   "OR",   "Output high when any input is high",
             [] { return std::make_unique<OrGate>(); });
        gate("NOT",  "NOT",  "Inverter",
             [] { return std::make_unique<NotGate>(); });
        gate("NAND", "NAND", "AND with an inverted output",
             [] { return std::make_unique<NandGate>(); });
        gate("NOR",  "NOR",  "OR with an inverted output",
             [] { return std::make_unique<NorGate>(); });
        gate("XOR",  "XOR",  "Output high when an odd number of inputs are high",
             [] { return std::make_unique<XorGate>(); });

        f.registerType({"SWITCH", "Switch", "I/O", "Input the user toggles; also a block input pin"},
                       [] { return std::make_unique<Switch>(); });
        f.registerType({"LED", "LED", "I/O", "Output lamp; also a block output pin"},
                       [] { return std::make_unique<Led>(); });
        f.registerType({"CLOCK", "Clock", "Sequential", "Square wave with an adjustable period"},
                       [] { return std::make_unique<Clock>(); });
        f.registerType({"DFF", "D flip-flop", "Sequential", "Latches D on the rising clock edge"},
                       [] { return std::make_unique<DFlipFlop>(); });
        f.registerType({"JKFF", "JK flip-flop", "Sequential",
                        "J sets, K resets, both toggle, on the rising clock edge"},
                       [] { return std::make_unique<JKFlipFlop>(); });
        f.registerType({"TFF", "T flip-flop", "Sequential", "Toggles on the rising clock edge while T is high"},
                       [] { return std::make_unique<TFlipFlop>(); });
        f.registerType({"MUX2", "2:1 multiplexer", "Multiplexers and decoders",
                        "S0 chooses D0 or D1"},
                       [] { return std::make_unique<Multiplexer>(2); });
        f.registerType({"MUX4", "4:1 multiplexer", "Multiplexers and decoders",
                        "S1 S0 choose one of D0 to D3"},
                       [] { return std::make_unique<Multiplexer>(4); });
        f.registerType({"SUBCIRCUIT", "Subcircuit", "Blocks", "A saved circuit used as one block"},
                       [] { return std::make_unique<SubCircuit>(); });
        return f;
    }();
    return registry;
}

} // namespace lb
