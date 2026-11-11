#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/CircuitBuilder.h"
#include "logicbench/Expression.h"
#include "logicbench/FlipFlop.h"
#include "logicbench/IoComponents.h"
#include "logicbench/SpecChecker.h"
#include "logicbench/TruthTable.h"

#include <algorithm>

using namespace lb;

namespace {

constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;

/// The RAD's CheckBeforeTheLab comparator, correct.
const char* kComparator =
    "GT = A1 & !B1 | !(A1 ^ B1) & A0 & !B0\n"
    "EQ = !(A1 ^ B1) & !(A0 ^ B0)\n"
    "LT = !(A1 & !B1 | !(A1 ^ B1) & A0 & !B0) & !(!(A1 ^ B1) & !(A0 ^ B0))\n";

/// Maya's version: GT looks only at the high bits.
const char* kBuggyComparator =
    "GT = A1 & !B1\n"
    "EQ = !(A1 ^ B1) & !(A0 ^ B0)\n"
    "LT = !(A1 & !B1 | !(A1 ^ B1) & A0 & !B0) & !(!(A1 ^ B1) & !(A0 ^ B0))\n";

std::unique_ptr<Circuit> build(const char* text)
{
    return CircuitBuilder::build(ExpressionParser::parse(text));
}

/// Dev's spec: the correct circuit's truth table over A1 A0 B1 B0, as CSV.
Specification exportSpec(Circuit& circuit, const std::vector<std::string>& inputs,
                         const std::vector<std::string>& outputs)
{
    std::vector<ComponentId> sw, led;
    for (const std::string& n : inputs)
        sw.push_back(TruthTableGenerator::byLabel(circuit, n, "SWITCH"));
    for (const std::string& n : outputs)
        led.push_back(TruthTableGenerator::byLabel(circuit, n, "LED"));
    return Specification::fromCsv(TruthTableGenerator::generate(circuit, sw, led).table.toCsv());
}

} // namespace

TEST(SpecCheckerTest, ACircuitPassesItsOwnExportedSpec)
{
    auto correct = build(kComparator);
    const Specification spec = exportSpec(*correct, {"A1", "A0", "B1", "B0"}, {"GT", "EQ", "LT"});
    EXPECT_EQ(spec.rowCount(), 16u);
    const SpecCheckResult r = SpecChecker::check(*correct, spec);
    EXPECT_TRUE(r.passed()) << r.message;
    EXPECT_EQ(r.rowsCompared, 16u);
    EXPECT_EQ(r.inputColumns, (std::vector<std::string>{"A1", "A0", "B1", "B0"}));
    EXPECT_EQ(r.outputColumns, (std::vector<std::string>{"GT", "EQ", "LT"}));
}

TEST(SpecCheckerTest, CheckBeforeTheLabFindsTheTwoGtRows)
{
    auto correct = build(kComparator);
    const Specification spec = exportSpec(*correct, {"A1", "A0", "B1", "B0"}, {"GT", "EQ", "LT"});

    auto buggy = build(kBuggyComparator);
    const SpecCheckResult r = SpecChecker::check(*buggy, spec);
    ASSERT_EQ(r.status, SpecCheckStatus::Fail);
    EXPECT_EQ(r.rowsDiffering, 2u);
    ASSERT_EQ(r.mismatches.size(), 2u);
    for (const Mismatch& m : r.mismatches) {
        EXPECT_EQ(m.output, "GT");
        EXPECT_EQ(m.expected, H);
        EXPECT_EQ(m.actual, L);
    }
    // A = 01, B = 00 and A = 11, B = 10 (inputs in the order A1 A0 B1 B0).
    EXPECT_EQ(r.mismatches[0].inputs, (std::vector<Signal>{L, H, L, L}));
    EXPECT_EQ(r.mismatches[1].inputs, (std::vector<Signal>{H, H, H, L}));
    EXPECT_NE(r.message.find("2 of 16"), std::string::npos);
}

TEST(SpecCheckerTest, ChangingOneGateFailsExactlyTheRowsThatChange)
{
    auto reference = build("Y = (A & B) | (C ^ D)");
    const Specification spec = exportSpec(*reference, {"A", "B", "C", "D"}, {"Y"});

    // Swap the AND for an OR.
    auto changed = build("Y = (A | B) | (C ^ D)");
    const SpecCheckResult r = SpecChecker::check(*changed, spec);
    ASSERT_EQ(r.status, SpecCheckStatus::Fail);

    std::size_t expected = 0;
    for (int row = 0; row < 16; ++row) {
        const bool a = row & 8, b = row & 4, c = row & 2, d = row & 1;
        if (((a && b) || (c != d)) != ((a || b) || (c != d)))
            ++expected;
    }
    EXPECT_EQ(r.rowsDiffering, expected);
}

TEST(SpecCheckerTest, AnUnmatchedColumnIsNamed)
{
    auto circuit = build("Y = A & B");
    const Specification spec = Specification::fromCsv("A,B2,Y\n0,0,0\n");
    const SpecCheckResult r = SpecChecker::check(*circuit, spec);
    EXPECT_EQ(r.status, SpecCheckStatus::UnmatchedColumn);
    EXPECT_NE(r.message.find("B2"), std::string::npos);
}

TEST(SpecCheckerTest, ColumnsMayComeInAnyOrderAndRowsMayBeASubset)
{
    auto circuit = build("Y = A & !B");
    // Inputs listed B first, and only two of the four rows.
    const Specification spec = Specification::fromCsv("Y,B,A\n1,0,1\n0,1,1\n");
    const SpecCheckResult r = SpecChecker::check(*circuit, spec);
    EXPECT_TRUE(r.passed()) << r.message;
    EXPECT_EQ(r.rowsCompared, 2u);
}

TEST(SpecCheckerTest, AnXInAnOutputColumnMeansEitherIsFine)
{
    auto circuit = build("Y = A ^ B");
    const Specification spec = Specification::fromCsv("A,B,Y\n0,0,X\n0,1,1\n1,0,1\n1,1,X\n");
    EXPECT_TRUE(SpecChecker::check(*circuit, spec).passed());
}

TEST(SpecCheckerTest, AnXInAnInputColumnIsRefused)
{
    auto circuit = build("Y = A ^ B");
    const SpecCheckResult r = SpecChecker::check(*circuit, Specification::fromCsv("A,B,Y\nX,0,1\n"));
    EXPECT_EQ(r.status, SpecCheckStatus::InputNotDefined);
    EXPECT_NE(r.message.find("Line 2"), std::string::npos) << r.message;
}

TEST(SpecCheckerTest, NeedsInputAndOutputColumns)
{
    auto circuit = build("Y = A ^ B");
    EXPECT_EQ(SpecChecker::check(*circuit, Specification::fromCsv("Y\n1\n")).status, SpecCheckStatus::NoInputs);
    EXPECT_EQ(SpecChecker::check(*circuit, Specification::fromCsv("A,B\n0,1\n")).status, SpecCheckStatus::NoOutputs);
}

TEST(SpecCheckerTest, RefusesSequentialCircuits)
{
    auto circuit = build("Y = A ^ B");
    circuit->add(std::make_unique<DFlipFlop>());
    const SpecCheckResult r = SpecChecker::check(*circuit, Specification::fromCsv("A,B,Y\n0,0,0\n"));
    EXPECT_EQ(r.status, SpecCheckStatus::SequentialPresent);
}

TEST(SpecCheckerTest, LeavesTheSwitchesAsTheyWere)
{
    auto circuit = build("Y = A & B");
    auto* a = static_cast<Switch*>(circuit->component(TruthTableGenerator::byLabel(*circuit, "A", "SWITCH")));
    a->setState(H);
    SpecChecker::check(*circuit, Specification::fromCsv("A,B,Y\n0,0,0\n1,1,1\n"));
    EXPECT_EQ(a->state(), H);
}

// ------------------------------------------------------- the CSV format ---

TEST(SpecCheckerTest, CsvRoundTripKeepsLabelsAndValues)
{
    TruthTable t;
    t.inputNames = {"A", "carry, in"};               // a label that needs quoting
    t.outputNames = {"Y \"out\""};
    t.inputs = {{L, L}, {L, H}, {H, L}, {H, H}};
    t.outputs = {{L}, {X}, {H}, {L}};
    const Specification spec = Specification::fromCsv(t.toCsv());
    EXPECT_EQ(spec.columns(), (std::vector<std::string>{"A", "carry, in", "Y \"out\""}));
    ASSERT_EQ(spec.rowCount(), 4u);
    EXPECT_EQ(spec.rows()[1], (std::vector<Signal>{L, H, X}));
    EXPECT_EQ(spec.rows()[2], (std::vector<Signal>{H, L, H}));
}

TEST(SpecCheckerTest, ToleratesSpacesBlankLinesLowercaseXAndABom)
{
    const Specification spec = Specification::fromCsv("\xEF\xBB\xBF A , Y \r\n\r\n 0 , x \r\n1,1\n");
    EXPECT_EQ(spec.columns(), (std::vector<std::string>{"A", "Y"}));
    EXPECT_EQ(spec.rows()[0], (std::vector<Signal>{L, X}));
    EXPECT_EQ(spec.rowLines(), (std::vector<int>{3, 4}));
}

TEST(SpecCheckerTest, MalformedFilesNameTheLine)
{
    const auto lineOf = [](const std::string& text) {
        try {
            Specification::fromCsv(text);
        } catch (const SpecFormatError& e) {
            return e.line();
        }
        return -1;
    };
    EXPECT_EQ(lineOf(""), 1);                                 // empty
    EXPECT_EQ(lineOf("A,B,Y\n"), 1);                          // header only
    EXPECT_EQ(lineOf("A,B,Y\n0,0,0\n0,1\n"), 3);              // too few values
    EXPECT_EQ(lineOf("A,B,Y\n0,0,0\n0,1,2\n"), 3);            // not 0, 1 or X
    EXPECT_EQ(lineOf("A,A,Y\n0,0,0\n"), 1);                   // repeated label
    EXPECT_EQ(lineOf("A,,Y\n0,0,0\n"), 1);                    // empty label
    EXPECT_EQ(lineOf("\"A,B\n0\n"), 1);                       // unclosed quote
}
