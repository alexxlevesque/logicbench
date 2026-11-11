#pragma once

#include "logicbench/SopMinimizer.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lb {

/// A rectangle of cells, in map coordinates.
struct CellRect {
    int row = 0;
    int column = 0;
    int rows = 1;
    int columns = 1;

    friend bool operator==(const CellRect& a, const CellRect& b) noexcept
    {
        return a.row == b.row && a.column == b.column && a.rows == b.rows && a.columns == b.columns;
    }
};

/// The layout of a Karnaugh map for 2 to 4 variables: which minterm sits in
/// which cell, the Gray-code axis labels, and where each term's loop goes.
///
/// The first half of the variables (rounded down) run down the rows, the rest
/// across the columns, both in Gray-code order (00, 01, 11, 10), so adjacent
/// cells differ in one variable, including across the edges.
class KarnaughMap {
public:
    /// n must be 2, 3 or 4.
    static KarnaughMap forInputs(int n);

    int variables() const noexcept { return m_variables; }
    int rows() const noexcept { return 1 << m_rowBits; }
    int columns() const noexcept { return 1 << m_columnBits; }
    int rowBits() const noexcept { return m_rowBits; }
    int columnBits() const noexcept { return m_columnBits; }

    /// Gray-code labels, e.g. "01" for the second row of a 4-variable map.
    std::string rowLabel(int row) const;
    std::string columnLabel(int column) const;

    std::uint32_t mintermAt(int row, int column) const;
    std::pair<int, int> cellOf(std::uint32_t minterm) const;

    /// The loop for one term: one rectangle, or two or four when it wraps
    /// around an edge of the map.
    std::vector<CellRect> loopsFor(const Implicant& term) const;

private:
    KarnaughMap(int variables, int rowBits, int columnBits);

    int m_variables;
    int m_rowBits;
    int m_columnBits;
};

} // namespace lb
