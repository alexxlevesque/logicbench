#include "logicbench/Simplifier.h"

namespace lb {

SimplifyResult Simplifier::simplify(Circuit& circuit, const std::vector<ComponentId>& switches,
                                    const std::vector<ComponentId>& leds)
{
    SimplifyResult result;
    if (static_cast<int>(switches.size()) > SopMinimizer::kMaxVariables) {
        result.status = SimplifyStatus::TooManyInputs;
        result.message = "Simplify works on up to " + std::to_string(SopMinimizer::kMaxVariables)
                       + " inputs; " + std::to_string(switches.size()) + " are selected.";
        return result;
    }

    TruthTableResult table = TruthTableGenerator::generate(circuit, switches, leds);
    switch (table.status) {
    case TruthTableStatus::Ok:
        break;
    case TruthTableStatus::NoInputsSelected:
        result.status = SimplifyStatus::NoInputsSelected;
        result.message = table.message;
        return result;
    case TruthTableStatus::NoOutputsSelected:
        result.status = SimplifyStatus::NoOutputsSelected;
        result.message = table.message;
        return result;
    case TruthTableStatus::TooManyInputs:
        result.status = SimplifyStatus::TooManyInputs;
        result.message = table.message;
        return result;
    case TruthTableStatus::SequentialPresent:
        result.status = SimplifyStatus::SequentialPresent;
        result.message = "Simplify needs a combinational circuit: this one holds state in a "
                         "flip-flop (or a block containing one), so its outputs are not a "
                         "function of the inputs alone.";
        return result;
    case TruthTableStatus::Oscillating:
        break;                       // checked row by row below
    }

    result.table = table.table;
    result.inputNames = table.table.inputNames;
    result.outputNames = table.table.outputNames;
    const int n = static_cast<int>(switches.size());

    std::vector<std::string> undefined;
    for (std::size_t out = 0; out < result.outputNames.size(); ++out) {
        std::vector<std::uint32_t> ones;
        bool defined = true;
        for (std::size_t row = 0; row < table.table.rowCount(); ++row) {
            const Signal s = table.table.outputs[row][out];
            if (s == Signal::Undefined)
                defined = false;
            else if (s == Signal::High)
                ones.push_back(static_cast<std::uint32_t>(row));   // rows are in binary order
        }
        if (!defined) {
            undefined.push_back(result.outputNames[out]);
            continue;
        }
        result.outputs.push_back(SopMinimizer::minimize(n, ones, {}, result.inputNames));
    }

    if (!undefined.empty()) {
        result.outputs.clear();
        result.status = SimplifyStatus::Unsettled;
        std::string list;
        for (const std::string& name : undefined)
            list += (list.empty() ? "" : ", ") + name;
        result.message = "Some rows of " + list + " read X (a floating input or a loop that does "
                         "not settle), so there is no single function to simplify. Connect every "
                         "input and try again.";
    }
    return result;
}

} // namespace lb
