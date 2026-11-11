#pragma once

#include "logicbench/Signal.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace lb {

/// Thrown with the 1-based line of the file that cannot be read.
class SpecFormatError : public std::runtime_error {
public:
    SpecFormatError(int line, const std::string& message)
        : std::runtime_error("line " + std::to_string(line) + ": " + message)
        , m_line(line), m_message(message) {}

    int line() const noexcept { return m_line; }
    const std::string& message() const noexcept { return m_message; }

private:
    int m_line;
    std::string m_message;
};

/// The expected behaviour of a circuit, as a truth table in CSV: a header row
/// of labels, then one row per input combination, each value 0, 1 or X.
///
/// It is the same format TruthTable::toCsv writes, so a TA makes one by
/// exporting the truth table of a correct circuit, and can fix it by hand in
/// any text editor. Which columns are inputs and which are outputs is not in
/// the file: SpecChecker decides by matching each label to a switch or an LED.
/// An X in an output column means "either is fine".
class Specification {
public:
    static Specification fromCsv(const std::string& text);
    static Specification load(const std::string& path);

    const std::vector<std::string>& columns() const noexcept { return m_columns; }
    const std::vector<std::vector<Signal>>& rows() const noexcept { return m_rows; }
    std::size_t rowCount() const noexcept { return m_rows.size(); }

    /// File line of each row, for messages.
    const std::vector<int>& rowLines() const noexcept { return m_lines; }

private:
    std::vector<std::string> m_columns;
    std::vector<std::vector<Signal>> m_rows;
    std::vector<int> m_lines;
};

} // namespace lb
