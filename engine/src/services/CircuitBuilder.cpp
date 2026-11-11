#include "logicbench/CircuitBuilder.h"

#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"

#include <algorithm>

namespace lb {

CircuitBuilder::CircuitBuilder(const ComponentFactory& factory)
    : m_factory(factory)
    , m_circuit(std::make_unique<Circuit>())
{
}

std::unique_ptr<Circuit> CircuitBuilder::build(const std::vector<Equation>& equations,
                                               const ComponentFactory& factory)
{
    CircuitBuilder builder(factory);
    for (const Equation& eq : equations) {
        const PinRef driver = eq.root->emitInto(builder);
        std::unique_ptr<Component> led = factory.create("LED");
        led->setLabel(eq.output);
        const ComponentId id = builder.m_circuit->add(std::move(led));
        builder.m_circuit->connect(driver, PinRef{id, 0});
        builder.m_depth[id] = -1;                // placed in the last column
        builder.m_order.push_back(id);
        builder.m_outputs[eq.output] = driver;
    }
    builder.layOut();
    builder.m_circuit->setName("built from expression");
    return std::move(builder.m_circuit);
}

PinRef CircuitBuilder::inputPin(const std::string& name)
{
    if (const auto out = m_outputs.find(name); out != m_outputs.end())
        return out->second;
    if (const auto in = m_switches.find(name); in != m_switches.end())
        return in->second;

    std::unique_ptr<Component> sw = m_factory.create("SWITCH");
    sw->setLabel(name);
    const ComponentId id = m_circuit->add(std::move(sw));
    m_depth[id] = 0;
    m_order.push_back(id);
    const PinRef pin{id, 0};
    m_switches[name] = pin;
    return pin;
}

PinRef CircuitBuilder::gate(const std::string& typeName, const std::vector<PinRef>& inputs,
                            const std::string& key)
{
    if (const auto it = m_shared.find(key); it != m_shared.end())
        return it->second;

    std::vector<PinRef> level = inputs;
    // AND, OR and XOR are associative, so a chain longer than one gate takes
    // becomes a small tree: fold just enough leading inputs into a gate that
    // the rest, plus its output, fit in one more. Ten inputs is a 3-input gate
    // feeding an 8-input one.
    const std::size_t most = static_cast<std::size_t>(Gate::kMaxInputs);
    while (level.size() > most) {
        const std::size_t take = std::min(most, level.size() - (most - 1));
        std::vector<PinRef> group(level.begin(), level.begin() + static_cast<long>(take));
        std::vector<PinRef> next{makeGate(typeName, group)};
        next.insert(next.end(), level.begin() + static_cast<long>(take), level.end());
        level = next;
    }
    const PinRef out = makeGate(typeName, level);
    m_shared[key] = out;
    return out;
}

PinRef CircuitBuilder::makeGate(const std::string& typeName, const std::vector<PinRef>& inputs)
{
    std::unique_ptr<Component> part = m_factory.create(typeName);
    if (typeName != "NOT")
        part->setProperty("inputs", std::to_string(inputs.size()));
    const ComponentId id = m_circuit->add(std::move(part));

    int depth = 0;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        m_circuit->connect(inputs[i], PinRef{id, static_cast<int>(i) + 1});   // pin 0 is the output
        depth = std::max(depth, m_depth[inputs[i].component]);
    }
    m_depth[id] = depth + 1;
    m_order.push_back(id);
    return PinRef{id, 0};
}

void CircuitBuilder::layOut()
{
    int deepest = 0;
    for (const auto& entry : m_depth)
        deepest = std::max(deepest, entry.second);

    std::map<int, int> rowsUsed;
    for (ComponentId id : m_order) {
        int column = m_depth[id];
        if (column < 0)
            column = deepest + 1;
        const int row = rowsUsed[column]++;
        m_circuit->component(id)->setPosition(column * kColumn, row * kRow);
    }
}

} // namespace lb
