#include "logicbench/SpecChecker.h"

#include "logicbench/TruthTable.h"

#include <set>

namespace lb {

SpecCheckResult SpecChecker::check(Circuit& circuit, const Specification& spec)
{
    SpecCheckResult result;
    std::vector<std::size_t> inputIndex;     // spec column of each input
    std::vector<std::size_t> outputIndex;
    std::vector<ComponentId> switches;
    std::vector<ComponentId> leds;

    for (std::size_t col = 0; col < spec.columns().size(); ++col) {
        const std::string& name = spec.columns()[col];
        if (const ComponentId sw = TruthTableGenerator::byLabel(circuit, name, "SWITCH"); sw != kNoComponent) {
            switches.push_back(sw);
            inputIndex.push_back(col);
            result.inputColumns.push_back(name);
        } else if (const ComponentId led = TruthTableGenerator::byLabel(circuit, name, "LED"); led != kNoComponent) {
            leds.push_back(led);
            outputIndex.push_back(col);
            result.outputColumns.push_back(name);
        } else {
            result.status = SpecCheckStatus::UnmatchedColumn;
            result.message = "No switch or LED is labelled " + name
                           + ". Label the circuit's inputs and outputs as the spec's header does.";
            return result;
        }
    }
    if (switches.empty()) {
        result.status = SpecCheckStatus::NoInputs;
        result.message = "None of the spec's columns is the label of a switch, so there is nothing to drive.";
        return result;
    }
    if (leds.empty()) {
        result.status = SpecCheckStatus::NoOutputs;
        result.message = "None of the spec's columns is the label of an LED, so there is nothing to compare.";
        return result;
    }
    for (std::size_t r = 0; r < spec.rowCount(); ++r) {
        for (std::size_t i = 0; i < inputIndex.size(); ++i) {
            if (spec.rows()[r][inputIndex[i]] == Signal::Undefined) {
                result.status = SpecCheckStatus::InputNotDefined;
                result.message = "Line " + std::to_string(spec.rowLines()[r]) + " gives input "
                               + result.inputColumns[i] + " as X; inputs must be 0 or 1.";
                return result;
            }
        }
    }

    const TruthTableResult table = TruthTableGenerator::generate(circuit, switches, leds);
    if (table.status == TruthTableStatus::SequentialPresent) {
        result.status = SpecCheckStatus::SequentialPresent;
        result.message = table.message;
        return result;
    }
    if (table.status == TruthTableStatus::TooManyInputs) {
        result.status = SpecCheckStatus::TooManyInputs;
        result.message = table.message;
        return result;
    }

    // The table's rows are in binary order over the switches, first switch
    // most significant, which is the spec's input column order.
    for (std::size_t r = 0; r < spec.rowCount(); ++r) {
        const std::vector<Signal>& specRow = spec.rows()[r];
        std::size_t index = 0;
        std::vector<Signal> inputs;
        for (std::size_t col : inputIndex) {
            index = (index << 1) | (specRow[col] == Signal::High ? 1u : 0u);
            inputs.push_back(specRow[col]);
        }
        ++result.rowsCompared;
        bool differs = false;
        for (std::size_t o = 0; o < outputIndex.size(); ++o) {
            const Signal expected = specRow[outputIndex[o]];
            if (expected == Signal::Undefined)
                continue;                                  // either value is fine
            const Signal actual = table.table.outputs[index][o];
            if (actual != expected) {
                differs = true;
                result.mismatches.push_back(
                    Mismatch{r, spec.rowLines()[r], inputs, result.outputColumns[o], expected, actual});
            }
        }
        if (differs)
            ++result.rowsDiffering;
    }

    if (result.mismatches.empty()) {
        result.status = SpecCheckStatus::Pass;
        result.message = "All " + std::to_string(result.rowsCompared) + " rows match the spec.";
    } else {
        std::set<std::string> outputs;
        for (const Mismatch& m : result.mismatches)
            outputs.insert(m.output);
        std::string list;
        for (const std::string& o : outputs)
            list += (list.empty() ? "" : ", ") + o;
        result.status = SpecCheckStatus::Fail;
        result.message = std::to_string(result.rowsDiffering) + " of " + std::to_string(result.rowsCompared)
                       + " rows differ, on " + list + ".";
    }
    return result;
}

} // namespace lb
