#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/Errors.h"

#include <memory>
#include <string>

namespace lb {

/// Reads and writes .lbc circuit files: JSON with a version number.
///
/// Version 1 files are still readable. That is not politeness — it is the
/// reason the format carries a version at all, and the loader branches on it
/// rather than guessing from the keys present.
///
/// Nothing here knows about Qt, so the file format is testable without a
/// display and a circuit can be loaded by a command-line tool.
class CircuitSerializer {
public:
    /// Written into every new file.
    static constexpr int kFormatVersion = 2;
    /// The oldest version this build can still read.
    static constexpr int kMinimumReadableVersion = 1;

    static std::string toJson(const Circuit& circuit, int indent = 2);

    /// Throws FileFormatError on anything that is not a readable circuit, and
    /// UnknownComponentType if the file names a component this build lacks.
    static std::unique_ptr<Circuit> fromJson(
        const std::string& text,
        const ComponentFactory& factory = ComponentFactory::standard());

    static void save(const Circuit& circuit, const std::string& path);

    static std::unique_ptr<Circuit> load(
        const std::string& path,
        const ComponentFactory& factory = ComponentFactory::standard());
};

} // namespace lb
