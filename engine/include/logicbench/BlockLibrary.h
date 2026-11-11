#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/SubCircuit.h"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace lb {

/// Thrown when a library operation cannot be done: a name already taken, a
/// block that does not exist, a folder that cannot be written.
class BlockLibraryError : public std::runtime_error {
public:
    explicit BlockLibraryError(const std::string& what) : std::runtime_error(what) {}
};

/// What the library shows for one block without the caller loading it.
struct BlockInfo {
    std::string name;                       ///< file name without .lbc
    std::filesystem::path path;
    std::vector<std::string> inputs;        ///< pin names, in pin order
    std::vector<std::string> outputs;
    std::string contents;                   ///< e.g. "1 XOR gate, 1 AND gate"
};

/// The folder of packaged blocks: one <name>.lbc per block, so blocks survive a
/// restart and appear in the palette in every session.
///
/// Names are file names, so a name may use letters, digits, spaces, '-' and
/// '_' and nothing else; the folder is created when the library is first
/// written. Every write goes to a temporary file first and is then renamed into
/// place, so a failed write never leaves half a block behind.
class BlockLibrary {
public:
    explicit BlockLibrary(std::filesystem::path directory);

    const std::filesystem::path& directory() const noexcept { return m_directory; }

    /// Every readable block, sorted by name. A file that is not a circuit is
    /// skipped, not fatal: one bad file should not empty the palette.
    std::vector<BlockInfo> list() const;

    bool contains(const std::string& name) const;

    /// Saves a circuit as a block. Throws if the name is invalid or taken, or
    /// if the circuit has no switch or no LED (a block needs pins).
    std::filesystem::path add(const Circuit& block, const std::string& name);

    void rename(const std::string& from, const std::string& to);
    void remove(const std::string& name);

    /// Copies a .lbc file into the library and returns the name it was given
    /// (its file name, with a number added if that is taken).
    std::string importFile(const std::filesystem::path& path);

    /// Names of the other blocks in the library that contain this one, at any
    /// depth. Deleting a block someone uses should ask first.
    std::vector<std::string> usersOf(const std::string& name) const;

    /// The block ready to place on a canvas.
    std::unique_ptr<SubCircuit> load(const std::string& name) const;

    static bool isValidName(const std::string& name);

private:
    std::filesystem::path pathFor(const std::string& name) const;
    void ensureDirectory() const;
    void writeAtomically(const std::filesystem::path& target, const std::string& text) const;

    std::filesystem::path m_directory;
};

} // namespace lb
