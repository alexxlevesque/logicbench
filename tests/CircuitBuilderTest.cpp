#include <gtest/gtest.h>

#include "logicbench/CircuitBuilder.h"
#include "logicbench/Expression.h"
#include "logicbench/IoComponents.h"
#include "logicbench/TruthTable.h"

#include <cmath>
#include <map>
#include <string>

using namespace lb;

namespace {

/// Builds the text, generates the circuit's truth table over its switches, and
/// checks every row against evaluating the equations directly.
void expectCircuitMatchesEquations(const std::string& text)
{
    const std::vector<Equation> eqs = ExpressionParser::parse(text);
    std::unique_ptr<Circuit> circuit = CircuitBuilder::build(eqs);

    const std::vector<ComponentId> switches = TruthTableGenerator::allSwitches(*circuit);
    const std::vector<ComponentId> leds = TruthTableGenerator::allLeds(*circuit);
    ASSERT_EQ(leds.size(), eqs.size()) << text;
    const TruthTableResult result = TruthTableGenerator::generate(*circuit, switches, leds);
    ASSERT_EQ(result.status, TruthTableStatus::Ok) << text << ": " << result.message;

    for (std::size_t row = 0; row < result.table.rowCount(); ++row) {
        std::map<std::string, Signal> values;
        for (std::size_t i = 0; i < switches.size(); ++i)
            values[result.table.inputNames[i]] = result.table.inputs[row][i];
        for (std::size_t e = 0; e < eqs.size(); ++e) {
            const Signal expected = eqs[e].root->evaluate(values);
            values[eqs[e].output] = expected;            // later lines may use it
            EXPECT_EQ(result.table.outputs[row][e], expected)
                << text << " row " << row << " output " << eqs[e].output;
        }
    }
}

std::size_t countOfType(const Circuit& c, const std::string& type)
{
    return c.componentsOfType(type).size();
}

} // namespace

TEST(CircuitBuilderTest, EveryTestEquationBuildsAMatchingCircuit)
{
    const char* cases[] = {
        "Y = A",
        "Y = !A",
        "Y = A & B",
        "Y = A | B",
        "Y = A ^ B",
        "Y = !(A & B)",
        "Y = A & B & C & D",
        "Y = A | B & !C",
        "Y = (A | B) & (C | D)",
        "S = A ^ B ^ Cin\nCout = (A & B) | (Cin & (A ^ B))",
        "GT = A1 & !B1 | !(A1 ^ B1) & A0 & !B0\nEQ = !(A1 ^ B1) & !(A0 ^ B0)\nLT = !GT & !EQ",
        "Y = A & A",
        "P = A ^ B\nS = P ^ C\nC2 = A & B | P & C",
        "Y = !(!(A | B) ^ !(C & D)) | E",
    };
    for (const char* text : cases)
        expectCircuitMatchesEquations(text);
}

TEST(CircuitBuilderTest, OneSwitchPerInputAndOneLabelledLedPerOutput)
{
    const auto circuit = CircuitBuilder::build(ExpressionParser::parse("S = A ^ B ^ Cin\nCout = A & B | Cin & (A ^ B)"));
    EXPECT_EQ(countOfType(*circuit, "SWITCH"), 3u);
    EXPECT_EQ(countOfType(*circuit, "LED"), 2u);
    std::vector<std::string> labels;
    for (Component* c : circuit->componentsOfType("SWITCH"))
        labels.push_back(c->label());
    EXPECT_EQ(labels, (std::vector<std::string>{"A", "B", "Cin"}));
}

TEST(CircuitBuilderTest, AChainOfOneOperatorIsOneGate)
{
    const auto circuit = CircuitBuilder::build(ExpressionParser::parse("Y = A ^ B ^ C"));
    ASSERT_EQ(countOfType(*circuit, "XOR"), 1u);
    EXPECT_EQ(circuit->componentsOfType("XOR").front()->inputPins().size(), 3u);
}

TEST(CircuitBuilderTest, ARepeatedSubexpressionIsBuiltOnce)
{
    // A ^ B appears in both equations; only one gate computes it.
    const auto circuit = CircuitBuilder::build(ExpressionParser::parse("S = (A ^ B) ^ C\nT = (A ^ B) & C"));
    // S flattens to one three-input XOR, so the shared (A ^ B) is in T only:
    // XORs are S's three-input one and T's two-input one.
    EXPECT_EQ(countOfType(*circuit, "XOR"), 2u);

    const auto shared = CircuitBuilder::build(ExpressionParser::parse("S = (A ^ B) & C\nT = (A ^ B) | C"));
    EXPECT_EQ(countOfType(*shared, "XOR"), 1u);
}

TEST(CircuitBuilderTest, MoreInputsThanAGateTakesBecomeATree)
{
    const auto circuit = CircuitBuilder::build(ExpressionParser::parse(
        "Y = A & B & C & D & E & F & G & H & I & J"));
    EXPECT_EQ(countOfType(*circuit, "SWITCH"), 10u);
    EXPECT_EQ(countOfType(*circuit, "AND"), 2u);        // eight in one, then that and the last two
    expectCircuitMatchesEquations("Y = A & B & C & D & E & F & G & H & I & J");
}

TEST(CircuitBuilderTest, LaysOutSwitchesLeftAndLedsRightByDepth)
{
    const auto circuit = CircuitBuilder::build(ExpressionParser::parse("Y = !(A & B) | C"));
    double switchX = -1, ledX = -1, maxGateX = -1;
    for (const auto& c : circuit->components()) {
        if (c->typeName() == "SWITCH")
            switchX = std::max(switchX, c->x());
        else if (c->typeName() == "LED")
            ledX = c->x();
        else
            maxGateX = std::max(maxGateX, c->x());
    }
    EXPECT_DOUBLE_EQ(switchX, 0.0);
    EXPECT_GT(maxGateX, switchX);
    EXPECT_GT(ledX, maxGateX);
    // Every part sits on the canvas grid.
    for (const auto& c : circuit->components()) {
        EXPECT_DOUBLE_EQ(std::fmod(c->x(), 10.0), 0.0);
        EXPECT_DOUBLE_EQ(std::fmod(c->y(), 10.0), 0.0);
    }
}
