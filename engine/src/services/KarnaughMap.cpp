#include "logicbench/KarnaughMap.h"

#include <stdexcept>

namespace lb {

namespace {

std::uint32_t gray(int i)
{
    return static_cast<std::uint32_t>(i ^ (i >> 1));
}

std::string bits(std::uint32_t value, int width)
{
    std::string text;
    for (int b = width - 1; b >= 0; --b)
        text += ((value >> b) & 1u) ? '1' : '0';
    return text;
}

/// The runs of consecutive true positions along one axis, unwrapped.
std::vector<std::pair<int, int>> runs(const std::vector<bool>& on)
{
    std::vector<std::pair<int, int>> result;
    const int n = static_cast<int>(on.size());
    for (int i = 0; i < n;) {
        if (!on[static_cast<std::size_t>(i)]) {
            ++i;
            continue;
        }
        int j = i;
        while (j < n && on[static_cast<std::size_t>(j)])
            ++j;
        result.emplace_back(i, j - i);
        i = j;
    }
    return result;
}

} // namespace

KarnaughMap::KarnaughMap(int variables, int rowBits, int columnBits)
    : m_variables(variables), m_rowBits(rowBits), m_columnBits(columnBits)
{
}

KarnaughMap KarnaughMap::forInputs(int n)
{
    if (n < 2 || n > 4)
        throw std::invalid_argument("a Karnaugh map is drawn for 2 to 4 inputs");
    return KarnaughMap(n, n / 2, n - n / 2);
}

std::string KarnaughMap::rowLabel(int row) const
{
    return bits(gray(row), m_rowBits);
}

std::string KarnaughMap::columnLabel(int column) const
{
    return bits(gray(column), m_columnBits);
}

std::uint32_t KarnaughMap::mintermAt(int row, int column) const
{
    // Row variables are the most significant bits of the minterm.
    return (gray(row) << m_columnBits) | gray(column);
}

std::pair<int, int> KarnaughMap::cellOf(std::uint32_t minterm) const
{
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < columns(); ++c)
            if (mintermAt(r, c) == minterm)
                return {r, c};
    return {-1, -1};
}

std::vector<CellRect> KarnaughMap::loopsFor(const Implicant& term) const
{
    // A product term covers a set of rows times a set of columns; each set is
    // contiguous on the torus, so it is at most two runs once unwrapped.
    std::vector<bool> rowOn(static_cast<std::size_t>(rows()), false);
    std::vector<bool> columnOn(static_cast<std::size_t>(columns()), false);
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < columns(); ++c)
            if (term.covers(mintermAt(r, c))) {
                rowOn[static_cast<std::size_t>(r)] = true;
                columnOn[static_cast<std::size_t>(c)] = true;
            }

    std::vector<CellRect> loops;
    for (const auto& r : runs(rowOn))
        for (const auto& c : runs(columnOn))
            loops.push_back(CellRect{r.first, c.first, r.second, c.second});
    return loops;
}

} // namespace lb
