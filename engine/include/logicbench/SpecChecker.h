#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/Specification.h"

#include <string>
#include <vector>

namespace lb {

enum class SpecCheckStatus {
    Pass,
    Fail,                 ///< at least one row differs; see mismatches
    UnmatchedColumn,      ///< a column is neither a switch nor an LED label
    NoInputs,             ///< no column matched a switch
    NoOutputs,            ///< no column matched an LED
    InputNotDefined,      ///< a spec row gives an input as X
    SequentialPresent,
    TooManyInputs
};

/// One output on one row that differs from the spec.
struct Mismatch {
    std::size_t specRow = 0;          ///< index into Specification::rows()
    int line = 0;                     ///< the row's line in the file
    std::vector<Signal> inputs;       ///< in the spec's input column order
    std::string output;
    Signal expected = Signal::Undefined;
    Signal actual = Signal::Undefined;
};

struct SpecCheckResult {
    SpecCheckStatus status = SpecCheckStatus::Pass;
    std::string message;
    std::vector<std::string> inputColumns;    ///< matched to switches, in file order
    std::vector<std::string> outputColumns;   ///< matched to LEDs, in file order
    std::vector<Mismatch> mismatches;
    std::size_t rowsCompared = 0;
    std::size_t rowsDiffering = 0;

    bool passed() const noexcept { return status == SpecCheckStatus::Pass; }
};

/// Compares a circuit with a specification, row by row.
///
/// Each column is matched by label: a switch's label makes it an input, an
/// LED's label an output. Then the circuit's truth table over exactly those
/// inputs is generated, and every spec row is compared output by output. The
/// circuit is left as it was found and the spec is never modified.
class SpecChecker {
public:
    static SpecCheckResult check(Circuit& circuit, const Specification& spec);
};

} // namespace lb
