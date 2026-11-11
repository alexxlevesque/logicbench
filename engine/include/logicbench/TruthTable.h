#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/Signal.h"

#include <string>
#include <vector>

namespace lb {

/// One generated table. Rows are in binary counting order over the inputs.
struct TruthTable {
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    std::vector<std::vector<Signal>> inputs;    ///< rows x inputNames
    std::vector<std::vector<Signal>> outputs;   ///< rows x outputNames

    std::size_t rowCount() const noexcept { return inputs.size(); }
    std::string toCsv() const;
};

enum class TruthTableStatus {
    Ok,
    NoInputsSelected,
    NoOutputsSelected,
    TooManyInputs,
    SequentialPresent,
    Oscillating       ///< table produced, but some rows could not settle
};

struct TruthTableResult {
    TruthTableStatus status = TruthTableStatus::Ok;
    TruthTable table;
    std::string message;

    /// True when a table was produced, even if some rows contain X.
    bool hasTable() const noexcept
    {
        return status == TruthTableStatus::Ok || status == TruthTableStatus::Oscillating;
    }
};

/// Enumerates every combination of the chosen switches, settles the circuit
/// for each and records the chosen LEDs.
///
/// The switch positions the user left on the canvas are restored afterwards,
/// so generating a table never disturbs the circuit being worked on.
class TruthTableGenerator {
public:
    /// 12 inputs is 4,096 rows. Past that the table stops being something a
    /// person reads, and the run stops being instant.
    static constexpr int kMaxInputs = 12;

    static TruthTableResult generate(Circuit& circuit,
                                     const std::vector<ComponentId>& switches,
                                     const std::vector<ComponentId>& leds);

    /// Every switch and LED in the circuit, in canvas order — the default
    /// selection the dialog opens with.
    static std::vector<ComponentId> allSwitches(const Circuit& circuit);
    static std::vector<ComponentId> allLeds(const Circuit& circuit);

    /// The first component of this type whose label is `label` (surrounding
    /// spaces ignored), or kNoComponent. SpecChecker matches columns this way.
    static ComponentId byLabel(const Circuit& circuit, const std::string& label,
                               const std::string& typeName);
};

} // namespace lb
