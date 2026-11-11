#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/Expression.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lb {

/// Turns parsed equations into a wired, laid-out circuit: one switch per input
/// name, one gate per operator (a chain of one operator becomes one gate with
/// several inputs), one LED per output, all labelled.
///
/// A sub-expression used twice is built once and shared, so S = A ^ B and
/// C = (A ^ B) & Cin share one XOR gate. Parts are placed in columns by depth:
/// switches on the left, each gate one column right of its deepest input, the
/// LEDs in the last column.
class CircuitBuilder {
public:
    static std::unique_ptr<Circuit> build(const std::vector<Equation>& equations,
                                          const ComponentFactory& factory = ComponentFactory::standard());

    /// Called by the expression nodes while they emit themselves.
    /// The output pin carrying `name`: an earlier equation's output if there
    /// is one, otherwise a switch labelled `name`, made on first use.
    PinRef inputPin(const std::string& name);

    /// The output pin of a gate of this type over these inputs. `key` names
    /// the sub-expression, so the same one asked for twice gives the same gate.
    /// More inputs than a gate takes are split into a tree of gates.
    PinRef gate(const std::string& typeName, const std::vector<PinRef>& inputs,
                const std::string& key);

    /// Grid spacing used by the layout, in canvas units.
    static constexpr double kColumn = 130.0;
    static constexpr double kRow = 70.0;

private:
    explicit CircuitBuilder(const ComponentFactory& factory);
    PinRef makeGate(const std::string& typeName, const std::vector<PinRef>& inputs);
    void layOut();

    const ComponentFactory& m_factory;
    std::unique_ptr<Circuit> m_circuit;
    std::map<std::string, PinRef> m_switches;
    std::map<std::string, PinRef> m_outputs;
    std::map<std::string, PinRef> m_shared;
    std::map<ComponentId, int> m_depth;
    std::vector<ComponentId> m_order;          ///< creation order, for the layout
};

} // namespace lb
