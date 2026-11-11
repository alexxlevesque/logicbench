#include "logicbench/Specification.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace lb {

namespace {

std::string trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return {};
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

/// Splits one CSV line, honouring double quotes ("a,b" and "" inside quotes),
/// which is how TruthTable::toCsv writes a label containing a comma.
std::vector<std::string> splitCsv(const std::string& line, int lineNumber)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                field += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }
    if (quoted)
        throw SpecFormatError(lineNumber, "a quoted label is never closed");
    fields.push_back(trim(field));
    return fields;
}

} // namespace

Specification Specification::fromCsv(const std::string& text)
{
    Specification spec;
    std::istringstream in(text);
    std::string line;
    int number = 0;
    bool haveHeader = false;

    while (std::getline(in, line)) {
        ++number;
        if (number == 1 && line.rfind("\xEF\xBB\xBF", 0) == 0)
            line.erase(0, 3);                          // a spreadsheet's byte-order mark
        if (trim(line).empty())
            continue;
        std::vector<std::string> fields = splitCsv(line, number);

        if (!haveHeader) {
            std::set<std::string> seen;
            for (const std::string& name : fields) {
                if (name.empty())
                    throw SpecFormatError(number, "a column in the header has no label");
                if (!seen.insert(name).second)
                    throw SpecFormatError(number, "the label " + name + " appears twice in the header");
            }
            spec.m_columns = fields;
            haveHeader = true;
            continue;
        }

        if (fields.size() != spec.m_columns.size()) {
            throw SpecFormatError(number, "this row has " + std::to_string(fields.size())
                                  + " values but the header has " + std::to_string(spec.m_columns.size())
                                  + " labels");
        }
        std::vector<Signal> row;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const std::string& v = fields[i];
            if (v == "0")
                row.push_back(Signal::Low);
            else if (v == "1")
                row.push_back(Signal::High);
            else if (v == "X" || v == "x")
                row.push_back(Signal::Undefined);
            else
                throw SpecFormatError(number, "the value for " + spec.m_columns[i] + " is \"" + v
                                      + "\"; each value must be 0, 1 or X");
        }
        spec.m_rows.push_back(std::move(row));
        spec.m_lines.push_back(number);
    }

    if (!haveHeader)
        throw SpecFormatError(1, "the file is empty; a spec starts with a header row of labels");
    if (spec.m_rows.empty())
        throw SpecFormatError(number, "the spec has a header but no rows");
    return spec;
}

Specification Specification::load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw SpecFormatError(0, "cannot open " + path);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return fromCsv(buffer.str());
}

} // namespace lb
