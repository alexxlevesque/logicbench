#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/SopMinimizer.h"
#include "logicbench/TruthTable.h"

#include <string>
#include <vector>

namespace lb {

enum class SimplifyStatus {
    Ok,
    NoInputsSelected,
    NoOutputsSelected,
    TooManyInputs,       ///< more than SopMinimizer::kMaxVariables
    SequentialPresent,
    Unsettled            ///< some row could not settle, so the function is not defined
};

struct SimplifyResult {
    SimplifyStatus status = SimplifyStatus::Ok;
    std::string message;
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    std::vector<SopResult> outputs;   ///< one per output, in order
    TruthTable table;

    bool ok() const noexcept { return status == SimplifyStatus::Ok; }
};

/// The minimal sum of products of each chosen output of a combinational
/// circuit, from its truth table.
///
/// A row that contains X (a floating input, an unsettled loop) means the
/// function is not fully defined, so nothing is minimized and the result says
/// which outputs were affected rather than guessing.
class Simplifier {
public:
    static SimplifyResult simplify(Circuit& circuit, const std::vector<ComponentId>& switches,
                                   const std::vector<ComponentId>& leds);
};

} // namespace lb
