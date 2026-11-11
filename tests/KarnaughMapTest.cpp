#include <gtest/gtest.h>

#include "logicbench/KarnaughMap.h"

#include <algorithm>
#include <set>

using namespace lb;

TEST(KarnaughMapTest, ShapesForTwoThreeAndFourInputs)
{
    const KarnaughMap two = KarnaughMap::forInputs(2);
    EXPECT_EQ(two.rows(), 2);
    EXPECT_EQ(two.columns(), 2);
    const KarnaughMap three = KarnaughMap::forInputs(3);
    EXPECT_EQ(three.rows(), 2);
    EXPECT_EQ(three.columns(), 4);
    const KarnaughMap four = KarnaughMap::forInputs(4);
    EXPECT_EQ(four.rows(), 4);
    EXPECT_EQ(four.columns(), 4);
    EXPECT_THROW(KarnaughMap::forInputs(1), std::invalid_argument);
    EXPECT_THROW(KarnaughMap::forInputs(5), std::invalid_argument);
}

TEST(KarnaughMapTest, AxisLabelsAreInGrayCodeOrder)
{
    const KarnaughMap four = KarnaughMap::forInputs(4);
    EXPECT_EQ(four.columnLabel(0), "00");
    EXPECT_EQ(four.columnLabel(1), "01");
    EXPECT_EQ(four.columnLabel(2), "11");
    EXPECT_EQ(four.columnLabel(3), "10");
    EXPECT_EQ(four.rowLabel(2), "11");
}

TEST(KarnaughMapTest, EveryMintermHasExactlyOneCell)
{
    for (int n = 2; n <= 4; ++n) {
        const KarnaughMap map = KarnaughMap::forInputs(n);
        std::set<std::uint32_t> seen;
        for (int r = 0; r < map.rows(); ++r)
            for (int c = 0; c < map.columns(); ++c) {
                const std::uint32_t m = map.mintermAt(r, c);
                EXPECT_TRUE(seen.insert(m).second);
                EXPECT_EQ(map.cellOf(m), std::make_pair(r, c));
            }
        EXPECT_EQ(seen.size(), 1u << n);
    }
}

TEST(KarnaughMapTest, NeighbouringCellsDifferInOneVariableAcrossEdgesToo)
{
    const KarnaughMap map = KarnaughMap::forInputs(4);
    const auto oneBit = [](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t d = a ^ b;
        return d != 0 && (d & (d - 1)) == 0;
    };
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            EXPECT_TRUE(oneBit(map.mintermAt(r, c), map.mintermAt(r, (c + 1) % 4)));
            EXPECT_TRUE(oneBit(map.mintermAt(r, c), map.mintermAt((r + 1) % 4, c)));
        }
}

TEST(KarnaughMapTest, AnInteriorTermIsOneLoop)
{
    // n = 4, variables A B C D; B·D covers the middle 2 x 2 block.
    const KarnaughMap map = KarnaughMap::forInputs(4);
    const Implicant bd{0b0101, 0b1010};
    const std::vector<CellRect> loops = map.loopsFor(bd);
    ASSERT_EQ(loops.size(), 1u);
    EXPECT_EQ(loops.front(), (CellRect{1, 1, 2, 2}));
}

TEST(KarnaughMapTest, AWrappingTermSplitsAtTheEdge)
{
    const KarnaughMap map = KarnaughMap::forInputs(4);
    // D' covers columns 00 and 10: the first and last, so two loops.
    const Implicant notD{0b0000, 0b1110};
    EXPECT_EQ(map.loopsFor(notD).size(), 2u);
    // B'·D' covers the four corners: four loops of one cell.
    const Implicant corners{0b0000, 0b1010};
    const std::vector<CellRect> loops = map.loopsFor(corners);
    ASSERT_EQ(loops.size(), 4u);
    for (const CellRect& r : loops) {
        EXPECT_EQ(r.rows, 1);
        EXPECT_EQ(r.columns, 1);
    }
}

TEST(KarnaughMapTest, LoopsCoverExactlyTheTermsCells)
{
    const KarnaughMap map = KarnaughMap::forInputs(3);
    const Implicant term{0b001, 0b100};   // B'·C
    std::set<std::uint32_t> inLoops;
    for (const CellRect& r : map.loopsFor(term))
        for (int i = 0; i < r.rows; ++i)
            for (int j = 0; j < r.columns; ++j)
                inLoops.insert(map.mintermAt(r.row + i, r.column + j));
    EXPECT_EQ(inLoops, (std::set<std::uint32_t>{1, 5}));
}
