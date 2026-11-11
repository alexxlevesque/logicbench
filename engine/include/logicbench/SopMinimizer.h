#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lb {

/// One product term over n variables. Variable 0 is the most significant bit
/// of a minterm number, the way a truth table is written: for inputs A, B, C,
/// minterm 6 is A=1 B=1 C=0.
struct Implicant {
    std::uint32_t value = 0;   ///< the fixed bits (zero where eliminated)
    std::uint32_t mask = 0;    ///< 1 where the variable has been eliminated

    bool covers(std::uint32_t minterm) const noexcept { return (minterm & ~mask) == value; }

    /// Number of literals in the term over `variables` inputs.
    int literals(int variables) const noexcept;

    friend bool operator==(const Implicant& a, const Implicant& b) noexcept
    {
        return a.value == b.value && a.mask == b.mask;
    }
    friend bool operator<(const Implicant& a, const Implicant& b) noexcept
    {
        return a.mask != b.mask ? a.mask < b.mask : a.value < b.value;
    }
};

/// A sum of products for one output.
struct SopResult {
    int variables = 0;
    std::vector<std::string> names;      ///< one per variable, most significant first
    std::vector<Implicant> terms;        ///< empty: the function is 0
    std::vector<std::uint32_t> minterms; ///< the function's 1 rows
    bool exact = true;                   ///< false if the cover search gave up and went greedy

    bool isConstantZero() const noexcept { return terms.empty(); }
    bool isConstantOne() const noexcept;
    int literalCount() const noexcept;

    /// Value of the expression on one row.
    bool evaluate(std::uint32_t row) const noexcept;

    /// The expression in ExpressionParser syntax, e.g. "(A & !B) | C", so
    /// Build minimized circuit can hand it straight to the builder. The
    /// constants, which the parser has no symbol for, come out as A & !A
    /// and A | !A over the first variable.
    std::string expression() const;

    /// For display: A·B′ + C, with 0 and 1 for the constants.
    std::string display() const;
};

/// Quine-McCluskey: every prime implicant, then the smallest set of them that
/// covers the function (fewest terms, then fewest literals). Essential primes
/// and dominated rows and columns are taken out first; what is left is searched
/// exactly, with a budget, and the best cover found so far is kept if the
/// budget runs out (exact = false). Up to 8 variables in well under a second.
class SopMinimizer {
public:
    static constexpr int kMaxVariables = 8;

    /// minterms and dontCares are row numbers below 2^variables.
    static SopResult minimize(int variables, const std::vector<std::uint32_t>& minterms,
                              const std::vector<std::uint32_t>& dontCares = {},
                              const std::vector<std::string>& names = {});

    /// Every prime implicant of the function, sorted.
    static std::vector<Implicant> primeImplicants(int variables,
                                                  const std::vector<std::uint32_t>& minterms,
                                                  const std::vector<std::uint32_t>& dontCares = {});
};

} // namespace lb
