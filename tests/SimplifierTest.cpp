#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/CircuitBuilder.h"
#include "logicbench/Expression.h"
#include "logicbench/FlipFlop.h"
#include "logicbench/Simplifier.h"

using namespace lb;

namespace {

std::unique_ptr<Circuit> buildText(const std::string& text)
{
    return CircuitBuilder::build(ExpressionParser::parse(text));
}

} // namespace

TEST(SimplifierTest, FullAdderCarryIsThreePairs)
{
    auto circuit = buildText("S = A ^ B ^ Cin\nCout = (A & B) | (Cin & (A ^ B))");
    const SimplifyResult r = Simplifier::simplify(*circuit, TruthTableGenerator::allSwitches(*circuit),
                                                  TruthTableGenerator::allLeds(*circuit));
    ASSERT_TRUE(r.ok()) << r.message;
    ASSERT_EQ(r.outputs.size(), 2u);
    EXPECT_EQ(r.outputNames, (std::vector<std::string>{"S", "Cout"}));
    EXPECT_EQ(r.outputs[0].terms.size(), 4u);       // three-input XOR does not reduce
    EXPECT_EQ(r.outputs[1].terms.size(), 3u);
    EXPECT_EQ(r.outputs[1].literalCount(), 6);
    EXPECT_EQ(r.outputs[1].names, (std::vector<std::string>{"A", "B", "Cin"}));
}

TEST(SimplifierTest, RedundantLogicSimplifies)
{
    // A·B + A·B' is just A.
    auto circuit = buildText("Y = A & B | A & !B");
    const SimplifyResult r = Simplifier::simplify(*circuit, TruthTableGenerator::allSwitches(*circuit),
                                                  TruthTableGenerator::allLeds(*circuit));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.outputs[0].expression(), "A");
}

TEST(SimplifierTest, RefusesSequentialCircuits)
{
    Circuit c;
    lbtest::buildHalfAdder(c);
    c.add(std::make_unique<DFlipFlop>());
    const SimplifyResult r = Simplifier::simplify(c, TruthTableGenerator::allSwitches(c),
                                                  TruthTableGenerator::allLeds(c));
    EXPECT_EQ(r.status, SimplifyStatus::SequentialPresent);
    EXPECT_FALSE(r.message.empty());
}

TEST(SimplifierTest, RefusesMoreThanEightInputs)
{
    auto circuit = buildText("Y = A & B & C & D & E & F & G & H & I");
    const SimplifyResult r = Simplifier::simplify(*circuit, TruthTableGenerator::allSwitches(*circuit),
                                                  TruthTableGenerator::allLeds(*circuit));
    EXPECT_EQ(r.status, SimplifyStatus::TooManyInputs);
}

TEST(SimplifierTest, AFloatingInputIsUnsettledNotGuessed)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    // A third, unconnected input on the AND gate makes the carry read X.
    c.setInputCount(h.andGate, 3);
    const SimplifyResult r = Simplifier::simplify(c, TruthTableGenerator::allSwitches(c),
                                                  TruthTableGenerator::allLeds(c));
    EXPECT_EQ(r.status, SimplifyStatus::Unsettled);
    EXPECT_NE(r.message.find('C'), std::string::npos);
    EXPECT_TRUE(r.outputs.empty());
}

TEST(SimplifierTest, NeedsInputsAndOutputs)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    EXPECT_EQ(Simplifier::simplify(c, {}, {h.sumLed}).status, SimplifyStatus::NoInputsSelected);
    EXPECT_EQ(Simplifier::simplify(c, {h.a}, {}).status, SimplifyStatus::NoOutputsSelected);
}
