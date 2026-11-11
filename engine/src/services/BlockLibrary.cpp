#include "logicbench/BlockLibrary.h"

#include "logicbench/CircuitSerializer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>

namespace lb {

namespace fs = std::filesystem;

namespace {

/// "2 XOR gates, 1 AND gate" for the parts of a block, in the order first seen.
std::string describeContents(const Circuit& circuit)
{
    std::vector<std::string> order;
    std::map<std::string, int> counts;
    for (const std::unique_ptr<Component>& c : circuit.components()) {
        const std::string type = c->typeName();
        if (type == "SWITCH" || type == "LED" || type == "NOTE")
            continue;                                    // the pins, not the contents
        std::string kind = type;
        if (const auto* sub = dynamic_cast<const SubCircuit*>(c.get()))
            kind = sub->blockName() + " block";
        else if (type == "AND" || type == "OR" || type == "NOT" || type == "NAND"
                 || type == "NOR" || type == "XOR")
            kind = type + " gate";
        if (counts[kind]++ == 0)
            order.push_back(kind);
    }
    std::string text;
    for (const std::string& kind : order) {
        if (!text.empty())
            text += ", ";
        const int n = counts[kind];
        text += std::to_string(n) + " " + kind + (n == 1 ? "" : "s");
    }
    return text.empty() ? "nothing but its pins" : text;
}

bool usesBlock(const Circuit& circuit, const std::string& name)
{
    for (const std::unique_ptr<Component>& c : circuit.components()) {
        if (const auto* sub = dynamic_cast<const SubCircuit*>(c.get())) {
            if (sub->blockName() == name || usesBlock(sub->inner(), name))
                return true;
        }
    }
    return false;
}

} // namespace

BlockLibrary::BlockLibrary(fs::path directory)
    : m_directory(std::move(directory))
{
}

bool BlockLibrary::isValidName(const std::string& name)
{
    if (name.empty() || name.size() > 64 || name.front() == ' ' || name.back() == ' ')
        return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ' ';
    });
}

fs::path BlockLibrary::pathFor(const std::string& name) const
{
    return m_directory / (name + ".lbc");
}

void BlockLibrary::ensureDirectory() const
{
    std::error_code ec;
    fs::create_directories(m_directory, ec);
    if (ec)
        throw BlockLibraryError("cannot create the block library folder " + m_directory.string()
                                + ": " + ec.message());
}

void BlockLibrary::writeAtomically(const fs::path& target, const std::string& text) const
{
    ensureDirectory();
    const fs::path temporary = target.string() + ".part";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!(out << text))
            throw BlockLibraryError("cannot write " + temporary.string());
    }
    std::error_code ec;
    fs::rename(temporary, target, ec);
    if (ec) {
        fs::remove(temporary, ec);
        throw BlockLibraryError("cannot write " + target.string());
    }
}

std::vector<BlockInfo> BlockLibrary::list() const
{
    std::vector<BlockInfo> blocks;
    std::error_code ec;
    if (!fs::is_directory(m_directory, ec))
        return blocks;
    for (const fs::directory_entry& entry : fs::directory_iterator(m_directory, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".lbc")
            continue;
        try {
            const std::string name = entry.path().stem().string();
            std::unique_ptr<SubCircuit> block = load(name);
            BlockInfo info;
            info.name = name;
            info.path = entry.path();
            for (std::size_t p = 0; p < block->pinCount(); ++p)
                (block->pin(p).isInput() ? info.inputs : info.outputs).push_back(block->pin(p).name());
            info.contents = describeContents(block->inner());
            blocks.push_back(std::move(info));
        } catch (const std::exception&) {
            // Not a circuit, or written by a newer build: leave it out.
        }
    }
    std::sort(blocks.begin(), blocks.end(),
              [](const BlockInfo& a, const BlockInfo& b) { return a.name < b.name; });
    return blocks;
}

bool BlockLibrary::contains(const std::string& name) const
{
    std::error_code ec;
    return isValidName(name) && fs::is_regular_file(pathFor(name), ec);
}

fs::path BlockLibrary::add(const Circuit& block, const std::string& name)
{
    if (!isValidName(name))
        throw BlockLibraryError("\"" + name + "\" cannot be a block name: use letters, digits, "
                                "spaces, '-' and '_'");
    if (contains(name))
        throw BlockLibraryError("the library already has a block called " + name);
    if (block.componentsOfType("SWITCH").empty() || block.componentsOfType("LED").empty())
        throw BlockLibraryError("a block needs at least one switch for an input pin and one LED "
                                "for an output pin");
    Circuit named(block);
    named.setName(name);
    const fs::path target = pathFor(name);
    writeAtomically(target, CircuitSerializer::toJson(named));
    return target;
}

void BlockLibrary::rename(const std::string& from, const std::string& to)
{
    if (!contains(from))
        throw BlockLibraryError("there is no block called " + from);
    if (!isValidName(to))
        throw BlockLibraryError("\"" + to + "\" cannot be a block name: use letters, digits, "
                                "spaces, '-' and '_'");
    if (from == to)
        return;
    if (contains(to))
        throw BlockLibraryError("the library already has a block called " + to);
    std::error_code ec;
    fs::rename(pathFor(from), pathFor(to), ec);
    if (ec)
        throw BlockLibraryError("cannot rename " + from + ": " + ec.message());
}

void BlockLibrary::remove(const std::string& name)
{
    if (!contains(name))
        throw BlockLibraryError("there is no block called " + name);
    std::error_code ec;
    fs::remove(pathFor(name), ec);
    if (ec)
        throw BlockLibraryError("cannot delete " + name + ": " + ec.message());
}

std::string BlockLibrary::importFile(const fs::path& path)
{
    std::unique_ptr<Circuit> circuit;
    try {
        circuit = CircuitSerializer::load(path.string());
    } catch (const std::exception& e) {
        throw BlockLibraryError(path.filename().string() + " cannot be imported: " + e.what());
    }

    std::string base;
    for (char c : path.stem().string())
        base += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ' ') ? c : '_';
    if (!isValidName(base))
        base = "block";
    std::string name = base;
    for (int n = 2; contains(name); ++n)
        name = base + " " + std::to_string(n);
    add(*circuit, name);
    return name;
}

std::vector<std::string> BlockLibrary::usersOf(const std::string& name) const
{
    std::vector<std::string> users;
    for (const BlockInfo& info : list()) {
        if (info.name == name)
            continue;
        try {
            if (usesBlock(*CircuitSerializer::load(info.path.string()), name))
                users.push_back(info.name);
        } catch (const std::exception&) {
        }
    }
    return users;
}

std::unique_ptr<SubCircuit> BlockLibrary::load(const std::string& name) const
{
    if (!contains(name))
        throw BlockLibraryError("there is no block called " + name);
    return std::make_unique<SubCircuit>(name, CircuitSerializer::load(pathFor(name).string()));
}

} // namespace lb
