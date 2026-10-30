#include "logicbench/TruthTable.h"

#include "logicbench/IoComponents.h"
#include "logicbench/Simulator.h"

#include <sstream>

namespace lb {

namespace {

std::string portName(const Component& c)
{
    return c.label().empty() ? (c.typeName() + " " + std::to_string(c.id())) : c.label();
}

std::string csvEscape(const std::string& field)
{
    if (field.find_first_of(",\"\n") == std::string::npos)
        return field;
    std::string quoted = "\"";
    for (char ch : field) {
        if (ch == '"')
            quoted += '"';
        quoted += ch;
    }
    return quoted + '"';
}

} // namespace

std::string TruthTable::toCsv() const
{
    std::ostringstream out;
    bool first = true;
    for (const std::string& name : inputNames) {
        out << (first ? "" : ",") << csvEscape(name);
        first = false;
    }
    for (const std::string& name : outputNames)
        out << "," << csvEscape(name);
    out << "\n";

    for (std::size_t row = 0; row < inputs.size(); ++row) {
        first = true;
        for (Signal s : inputs[row]) {
            out << (first ? "" : ",") << toChar(s);
            first = false;
        }
        for (Signal s : outputs[row])
            out << "," << toChar(s);
        out << "\n";
    }
    return out.str();
}

std::vector<ComponentId> TruthTableGenerator::allSwitches(const Circuit& circuit)
{
    std::vector<ComponentId> ids;
    for (Component* c : circuit.componentsOfType("SWITCH"))
        ids.push_back(c->id());
    return ids;
}

std::vector<ComponentId> TruthTableGenerator::allLeds(const Circuit& circuit)
{
    std::vector<ComponentId> ids;
    for (Component* c : circuit.componentsOfType("LED"))
        ids.push_back(c->id());
    return ids;
}

TruthTableResult TruthTableGenerator::generate(Circuit& circuit,
                                               const std::vector<ComponentId>& switches,
                                               const std::vector<ComponentId>& leds)
{
    TruthTableResult result;

    if (switches.empty()) {
        result.status = TruthTableStatus::NoInputsSelected;
        result.message = "Select at least one input switch.";
        return result;
    }
    if (leds.empty()) {
        result.status = TruthTableStatus::NoOutputsSelected;
        result.message = "Select at least one output LED.";
        return result;
    }
    if (static_cast<int>(switches.size()) > kMaxInputs) {
        result.status = TruthTableStatus::TooManyInputs;
        result.message = "A truth table is limited to " + std::to_string(kMaxInputs)
                       + " inputs; " + std::to_string(switches.size()) + " are selected.";
        return result;
    }
    if (circuit.hasSequentialParts()) {
        result.status = TruthTableStatus::SequentialPresent;
        result.message = "This circuit contains sequential parts (a flip-flop or a block "
                         "holding one), so its outputs depend on history as well as on the "
                         "inputs. Use the timing diagram instead.";
        return result;
    }

    std::vector<Switch*> inputs;
    inputs.reserve(switches.size());
    for (ComponentId id : switches) {
        auto* sw = dynamic_cast<Switch*>(circuit.component(id));
        if (!sw) {
            result.status = TruthTableStatus::NoInputsSelected;
            result.message = "An input in the selection is no longer a switch in this circuit.";
            return result;
        }
        inputs.push_back(sw);
        result.table.inputNames.push_back(portName(*sw));
    }

    std::vector<Component*> outputs;
    outputs.reserve(leds.size());
    for (ComponentId id : leds) {
        Component* led = circuit.component(id);
        if (!led || led->typeName() != "LED") {
            result.status = TruthTableStatus::NoOutputsSelected;
            result.message = "An output in the selection is no longer an LED in this circuit.";
            return result;
        }
        outputs.push_back(led);
        result.table.outputNames.push_back(portName(*led));
    }

    // Borrow the circuit, then hand it back exactly as it was found.
    std::vector<Signal> savedStates;
    savedStates.reserve(inputs.size());
    for (Switch* sw : inputs)
        savedStates.push_back(sw->state());

    Simulator simulator(circuit);
    const std::size_t rows = std::size_t{1} << inputs.size();
    bool sawOscillation = false;

    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t bit = 0; bit < inputs.size(); ++bit) {
            // Most significant selected input first, so the table counts the
            // way it is written on paper.
            const std::size_t shift = inputs.size() - 1 - bit;
            inputs[bit]->setState(signalFromBool((row >> shift) & 1u));
        }

        circuit.reset();
        const SettleReport report = simulator.reset();
        if (!report.stable())
            sawOscillation = true;

        std::vector<Signal> inputRow;
        inputRow.reserve(inputs.size());
        for (Switch* sw : inputs)
            inputRow.push_back(sw->state());

        std::vector<Signal> outputRow;
        outputRow.reserve(outputs.size());
        for (Component* led : outputs)
            outputRow.push_back(led->pin(0).signal());

        result.table.inputs.push_back(std::move(inputRow));
        result.table.outputs.push_back(std::move(outputRow));
    }

    for (std::size_t i = 0; i < inputs.size(); ++i)
        inputs[i]->setState(savedStates[i]);
    circuit.reset();
    simulator.reset();

    if (sawOscillation) {
        result.status = TruthTableStatus::Oscillating;
        result.message = "Some rows contain a feedback loop that never settles. Those "
                         "outputs are shown as X.";
    }
    return result;
}

} // namespace lb
