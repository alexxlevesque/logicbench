#include <gtest/gtest.h>

#include "logicbench/Gates.h"

#include <memory>

using namespace lb;

namespace {

/// Drives the inputs of a standalone gate and returns its output pin.
/// Pin 0 is the output; inputs start at pin 1.
Signal apply(Gate& gate, std::initializer_list<Signal> inputs)
{
    int index = 1;
    for (Signal s : inputs)
        gate.pin(static_cast<std::size_t>(index++)).setSignal(s);
    gate.evaluate();
    return gate.pin(0).signal();
}

constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;

} // namespace

TEST(GateTest, AndFullTruthTable)
{
    AndGate g;
    EXPECT_EQ(apply(g, {L, L}), L);
    EXPECT_EQ(apply(g, {L, H}), L);
    EXPECT_EQ(apply(g, {H, L}), L);
    EXPECT_EQ(apply(g, {H, H}), H);
}

TEST(GateTest, OrFullTruthTable)
{
    OrGate g;
    EXPECT_EQ(apply(g, {L, L}), L);
    EXPECT_EQ(apply(g, {L, H}), H);
    EXPECT_EQ(apply(g, {H, L}), H);
    EXPECT_EQ(apply(g, {H, H}), H);
}

TEST(GateTest, XorFullTruthTable)
{
    XorGate g;
    EXPECT_EQ(apply(g, {L, L}), L);
    EXPECT_EQ(apply(g, {L, H}), H);
    EXPECT_EQ(apply(g, {H, L}), H);
    EXPECT_EQ(apply(g, {H, H}), L);
}

TEST(GateTest, NandIsAndInverted)
{
    NandGate g;
    EXPECT_EQ(apply(g, {L, L}), H);
    EXPECT_EQ(apply(g, {L, H}), H);
    EXPECT_EQ(apply(g, {H, L}), H);
    EXPECT_EQ(apply(g, {H, H}), L);
}

TEST(GateTest, NorIsOrInverted)
{
    NorGate g;
    EXPECT_EQ(apply(g, {L, L}), H);
    EXPECT_EQ(apply(g, {L, H}), L);
    EXPECT_EQ(apply(g, {H, L}), L);
    EXPECT_EQ(apply(g, {H, H}), L);
}

TEST(GateTest, NotFullTruthTable)
{
    NotGate g;
    EXPECT_EQ(g.inputCount(), 1);
    EXPECT_EQ(apply(g, {L}), H);
    EXPECT_EQ(apply(g, {H}), L);
    EXPECT_EQ(apply(g, {X}), X);
}

TEST(GateTest, FloatingInputProducesUndefinedUnlessDominated)
{
    AndGate a;
    EXPECT_EQ(apply(a, {H, X}), X);   // cannot tell
    EXPECT_EQ(apply(a, {L, X}), L);   // zero wins regardless

    OrGate o;
    EXPECT_EQ(apply(o, {L, X}), X);
    EXPECT_EQ(apply(o, {H, X}), H);   // one wins regardless
}

TEST(GateTest, FreshGateOutputIsUndefinedBeforeEvaluation)
{
    AndGate g;
    EXPECT_EQ(g.pin(0).signal(), Signal::Undefined);
}

TEST(GateTest, EvaluateReportsWhetherTheOutputChanged)
{
    AndGate g;
    g.pin(1).setSignal(H);
    g.pin(2).setSignal(H);
    EXPECT_TRUE(g.evaluate());    // Undefined -> High
    EXPECT_FALSE(g.evaluate());   // already High, nothing to wake up
}

TEST(GateTest, InputCountGrowsAndFoldsAcrossEveryInput)
{
    AndGate g;
    g.setInputCount(3);
    EXPECT_EQ(g.inputCount(), 3);
    EXPECT_EQ(g.pinCount(), 4u);
    EXPECT_EQ(apply(g, {H, H, H}), H);
    EXPECT_EQ(apply(g, {H, H, L}), L);

    OrGate o;
    o.setInputCount(4);
    EXPECT_EQ(apply(o, {L, L, L, H}), H);
    EXPECT_EQ(apply(o, {L, L, L, L}), L);
}

TEST(GateTest, ThreeInputXorIsOddParity)
{
    XorGate g;
    g.setInputCount(3);
    EXPECT_EQ(apply(g, {H, L, L}), H);
    EXPECT_EQ(apply(g, {H, H, L}), L);
    EXPECT_EQ(apply(g, {H, H, H}), H);
}

TEST(GateTest, InputCountIsClampedToTheLegalRange)
{
    AndGate g;
    g.setInputCount(1);                    // below the minimum for AND
    EXPECT_EQ(g.inputCount(), 2);
    g.setInputCount(99);
    EXPECT_EQ(g.inputCount(), Gate::kMaxInputs);

    NotGate n;
    n.setInputCount(5);                    // NOT has exactly one input
    EXPECT_EQ(n.inputCount(), 1);
}

TEST(GateTest, OutputPinIndexSurvivesAnInputCountChange)
{
    // Wires leaving a gate refer to pin 0, so growing the input list must not
    // renumber the output.
    NandGate g;
    EXPECT_TRUE(g.pin(0).isOutput());
    g.setInputCount(5);
    EXPECT_TRUE(g.pin(0).isOutput());
    EXPECT_EQ(g.pin(0).name(), "Y");
}

TEST(GateTest, CloneProducesAnIndependentCopyOfTheSameType)
{
    NandGate original;
    original.setInputCount(3);
    original.setLabel("carry");

    std::unique_ptr<Component> copy = original.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->typeName(), "NAND");
    EXPECT_EQ(copy->label(), "carry");
    EXPECT_EQ(copy->pinCount(), original.pinCount());

    // Independent: changing the copy leaves the original alone.
    copy->setLabel("other");
    EXPECT_EQ(original.label(), "carry");
}

TEST(GateTest, PropertiesRoundTripThroughStrings)
{
    OrGate g;
    EXPECT_TRUE(g.setProperty("inputs", "4"));
    EXPECT_TRUE(g.setProperty("label", "sum"));
    EXPECT_TRUE(g.setProperty("delay", "3"));
    EXPECT_EQ(g.properties().at("inputs"), "4");
    EXPECT_EQ(g.properties().at("label"), "sum");
    EXPECT_EQ(g.properties().at("delay"), "3");
    EXPECT_FALSE(g.setProperty("nonsense", "1"));
    EXPECT_FALSE(g.setProperty("inputs", "not a number"));
}
