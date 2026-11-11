#include <gtest/gtest.h>

#include "logicbench/Expression.h"
#include "logicbench/SopMinimizer.h"

#include <chrono>
#include <map>
#include <random>
#include <set>

using namespace lb;

namespace {

/// Checks the result computes exactly the function, both directly and through
/// its parser-syntax expression (which is what Build minimized circuit uses).
void expectSameFunction(const SopResult& r, int n, const std::set<std::uint32_t>& ones)
{
    const auto parsed = ExpressionParser::parse("Y = " + r.expression());
    for (std::uint32_t row = 0; row < (1u << n); ++row) {
        const bool want = ones.count(row) != 0;
        EXPECT_EQ(r.evaluate(row), want) << "row " << row << " of " << r.display();
        std::map<std::string, Signal> values;
        for (int v = 0; v < n; ++v)
            values[r.names[static_cast<std::size_t>(v)]] = signalFromBool((row >> (n - 1 - v)) & 1u);
        EXPECT_EQ(parsed.front().root->evaluate(values), signalFromBool(want))
            << "row " << row << " of " << r.expression();
    }
}

/// The fewest terms any set of prime implicants can cover the function with,
/// by trying every subset. Only for tiny functions.
std::size_t bruteForceTerms(int n, const std::vector<std::uint32_t>& ones)
{
    const std::vector<Implicant> primes = SopMinimizer::primeImplicants(n, ones);
    std::size_t best = primes.size();
    for (std::uint32_t subset = 0; subset < (1u << primes.size()); ++subset) {
        std::size_t count = 0;
        for (std::size_t p = 0; p < primes.size(); ++p)
            count += (subset >> p) & 1u;
        if (count >= best)
            continue;
        bool all = true;
        for (std::uint32_t m : ones) {
            bool hit = false;
            for (std::size_t p = 0; p < primes.size() && !hit; ++p)
                hit = ((subset >> p) & 1u) && primes[p].covers(m);
            all = all && hit;
        }
        if (all)
            best = count;
    }
    return best;
}

} // namespace

TEST(SopMinimizerTest, MajorityIsThreePairs)
{
    const SopResult r = SopMinimizer::minimize(3, {3, 5, 6, 7}, {}, {"A", "B", "Cin"});
    EXPECT_EQ(r.terms.size(), 3u);
    EXPECT_EQ(r.literalCount(), 6);
    EXPECT_TRUE(r.exact);
    expectSameFunction(r, 3, {3, 5, 6, 7});
}

TEST(SopMinimizerTest, XorDoesNotReduce)
{
    const SopResult r = SopMinimizer::minimize(2, {1, 2});
    EXPECT_EQ(r.terms.size(), 2u);
    EXPECT_EQ(r.literalCount(), 4);
    EXPECT_EQ(r.display(), "A·B′ + A′·B");
}

TEST(SopMinimizerTest, AdjacentMintermsMerge)
{
    // A'B'C' + A'B'C + A'BC' + A'BC = A'
    const SopResult r = SopMinimizer::minimize(3, {0, 1, 2, 3});
    ASSERT_EQ(r.terms.size(), 1u);
    EXPECT_EQ(r.literalCount(), 1);
    EXPECT_EQ(r.expression(), "!A");
}

TEST(SopMinimizerTest, ConstantsAreRecognisedAndStillBuildable)
{
    const SopResult zero = SopMinimizer::minimize(2, {});
    EXPECT_TRUE(zero.isConstantZero());
    EXPECT_EQ(zero.display(), "0");
    expectSameFunction(zero, 2, {});

    const SopResult one = SopMinimizer::minimize(2, {0, 1, 2, 3});
    EXPECT_TRUE(one.isConstantOne());
    EXPECT_EQ(one.display(), "1");
    expectSameFunction(one, 2, {0, 1, 2, 3});
}

TEST(SopMinimizerTest, DontCaresAreUsedButNeverRequired)
{
    // BCD digit > 4 on inputs 8 4 2 1, with 10..15 as don't-cares: A + BD + BC.
    const SopResult r = SopMinimizer::minimize(4, {5, 6, 7, 8, 9}, {10, 11, 12, 13, 14, 15});
    EXPECT_EQ(r.terms.size(), 3u);
    EXPECT_EQ(r.literalCount(), 5);
    for (std::uint32_t row : {5u, 6u, 7u, 8u, 9u})
        EXPECT_TRUE(r.evaluate(row));
    for (std::uint32_t row : {0u, 1u, 2u, 3u, 4u})
        EXPECT_FALSE(r.evaluate(row));
}

TEST(SopMinimizerTest, EveryThreeInputFunctionUsesTheFewestTerms)
{
    for (std::uint32_t f = 0; f < 256; ++f) {
        std::vector<std::uint32_t> ones;
        for (std::uint32_t row = 0; row < 8; ++row)
            if ((f >> row) & 1u)
                ones.push_back(row);
        const SopResult r = SopMinimizer::minimize(3, ones);
        EXPECT_EQ(r.terms.size(), bruteForceTerms(3, ones)) << "function " << f;
        expectSameFunction(r, 3, std::set<std::uint32_t>(ones.begin(), ones.end()));
    }
}

TEST(SopMinimizerTest, TwoHundredRandomFunctionsKeepTheirTruthTableAndShrink)
{
    std::mt19937 rng(320);
    for (int trial = 0; trial < 200; ++trial) {
        const int n = 1 + trial % SopMinimizer::kMaxVariables;
        std::set<std::uint32_t> ones;
        for (std::uint32_t row = 0; row < (1u << n); ++row)
            if (rng() % 2)
                ones.insert(row);
        const SopResult r = SopMinimizer::minimize(n, std::vector<std::uint32_t>(ones.begin(), ones.end()));
        expectSameFunction(r, n, ones);
        // No more literals than the canonical sum of minterms.
        EXPECT_LE(r.literalCount(), static_cast<int>(ones.size()) * n) << "trial " << trial;
        EXPECT_LE(r.terms.size(), ones.size());
    }
}

TEST(SopMinimizerTest, EightInputsFinishInWellUnderASecond)
{
    std::mt19937 rng(1234);
    std::vector<std::uint32_t> ones;
    for (std::uint32_t row = 0; row < 256; ++row)
        if (rng() % 2)
            ones.push_back(row);
    const auto start = std::chrono::steady_clock::now();
    const SopResult r = SopMinimizer::minimize(8, ones);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1000);
    EXPECT_FALSE(r.terms.empty());
}
