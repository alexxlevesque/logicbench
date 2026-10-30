#include <gtest/gtest.h>

#include "logicbench/Signal.h"

#include <sstream>

using lb::Signal;

namespace {
constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;
} // namespace

TEST(SignalTest, AndTruthTableIncludingUndefined)
{
    EXPECT_EQ(L & L, L);
    EXPECT_EQ(L & H, L);
    EXPECT_EQ(H & L, L);
    EXPECT_EQ(H & H, H);
    // A dominant zero decides the result even when the other input is unknown.
    EXPECT_EQ(L & X, L);
    EXPECT_EQ(X & L, L);
    EXPECT_EQ(H & X, X);
    EXPECT_EQ(X & H, X);
    EXPECT_EQ(X & X, X);
}

TEST(SignalTest, OrTruthTableIncludingUndefined)
{
    EXPECT_EQ(L | L, L);
    EXPECT_EQ(L | H, H);
    EXPECT_EQ(H | L, H);
    EXPECT_EQ(H | H, H);
    EXPECT_EQ(H | X, H);   // dominant one
    EXPECT_EQ(X | H, H);
    EXPECT_EQ(L | X, X);
    EXPECT_EQ(X | L, X);
    EXPECT_EQ(X | X, X);
}

TEST(SignalTest, XorNeedsBothOperandsKnown)
{
    EXPECT_EQ(L ^ L, L);
    EXPECT_EQ(L ^ H, H);
    EXPECT_EQ(H ^ L, H);
    EXPECT_EQ(H ^ H, L);
    EXPECT_EQ(H ^ X, X);
    EXPECT_EQ(L ^ X, X);
    EXPECT_EQ(X ^ X, X);
}

TEST(SignalTest, NotLeavesUndefinedUndefined)
{
    EXPECT_EQ(!L, H);
    EXPECT_EQ(!H, L);
    EXPECT_EQ(!X, X);
}

TEST(SignalTest, CompoundAssignmentMatchesBinaryForm)
{
    Signal a = H;
    a &= L;
    EXPECT_EQ(a, L);
    a |= H;
    EXPECT_EQ(a, H);
    a ^= H;
    EXPECT_EQ(a, L);
}

TEST(SignalTest, CharacterRoundTrip)
{
    EXPECT_EQ(lb::toChar(L), '0');
    EXPECT_EQ(lb::toChar(H), '1');
    EXPECT_EQ(lb::toChar(X), 'X');
    EXPECT_EQ(lb::signalFromChar('0'), L);
    EXPECT_EQ(lb::signalFromChar('1'), H);
    EXPECT_EQ(lb::signalFromChar('?'), X);
}

TEST(SignalTest, StreamsAsASingleCharacter)
{
    std::ostringstream out;
    out << H << L << X;
    EXPECT_EQ(out.str(), "10X");
}

TEST(SignalTest, OperatorsAreUsableAtCompileTime)
{
    static_assert((H & H) == H, "and is constexpr");
    static_assert((L | X) == X, "or is constexpr");
    static_assert(!X == X, "not is constexpr");
    SUCCEED();
}
