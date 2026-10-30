#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/DFlipFlop.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/TruthTable.h"

using namespace lb;

TEST(TruthTableTest, HalfAdderTableIsCorrectAndInCountingOrder)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);

    const TruthTableResult result = TruthTableGenerator::generate(
        c, {h.a, h.b}, {h.carryLed, h.sumLed});

    ASSERT_EQ(result.status, TruthTableStatus::Ok);
    ASSERT_EQ(result.table.rowCount(), 4u);
    EXPECT_EQ(result.table.inputNames, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(result.table.outputNames, (std::vector<std::string>{"C", "S"}));

    const Signal L = Signal::Low;
    const Signal H = Signal::High;
    EXPECT_EQ(result.table.inputs[0], (std::vector<Signal>{L, L}));
    EXPECT_EQ(result.table.outputs[0], (std::vector<Signal>{L, L}));
    EXPECT_EQ(result.table.inputs[1], (std::vector<Signal>{L, H}));
    EXPECT_EQ(result.table.outputs[1], (std::vector<Signal>{L, H}));
    EXPECT_EQ(result.table.inputs[2], (std::vector<Signal>{H, L}));
    EXPECT_EQ(result.table.outputs[2], (std::vector<Signal>{L, H}));
    EXPECT_EQ(result.table.inputs[3], (std::vector<Signal>{H, H}));
    EXPECT_EQ(result.table.outputs[3], (std::vector<Signal>{H, L}));
}

TEST(TruthTableTest, SwitchPositionsAreRestoredAfterwards)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    auto* a = dynamic_cast<Switch*>(c.component(h.a));
    a->setState(Signal::High);

    TruthTableGenerator::generate(c, {h.a, h.b}, {h.sumLed});
    EXPECT_EQ(a->state(), Signal::High) << "generating a table must not disturb the canvas";
}

TEST(TruthTableTest, RefusesOnSequentialParts)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    c.add(std::make_unique<DFlipFlop>());

    const TruthTableResult result = TruthTableGenerator::generate(c, {h.a}, {h.sumLed});
    EXPECT_EQ(result.status, TruthTableStatus::SequentialPresent);
    EXPECT_FALSE(result.hasTable());
    EXPECT_FALSE(result.message.empty());
}

TEST(TruthTableTest, RefusesOnMoreThanTwelveInputs)
{
    Circuit c;
    std::vector<ComponentId> switches;
    for (int i = 0; i < 13; ++i)
        switches.push_back(c.add(std::make_unique<Switch>()));
    const ComponentId led = c.add(std::make_unique<Led>());

    const TruthTableResult result = TruthTableGenerator::generate(c, switches, {led});
    EXPECT_EQ(result.status, TruthTableStatus::TooManyInputs);
    EXPECT_NE(result.message.find("12"), std::string::npos);
}

TEST(TruthTableTest, RefusesWithNothingSelected)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    EXPECT_EQ(TruthTableGenerator::generate(c, {}, {h.sumLed}).status,
              TruthTableStatus::NoInputsSelected);
    EXPECT_EQ(TruthTableGenerator::generate(c, {h.a}, {}).status,
              TruthTableStatus::NoOutputsSelected);
}

TEST(TruthTableTest, OscillatingCircuitStillProducesATableMarkedX)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId orGate = c.add(std::make_unique<OrGate>());
    const ComponentId inverter = c.add(std::make_unique<NotGate>());
    const ComponentId led = c.add(std::make_unique<Led>());
    c.component(sw)->setLabel("A");
    c.component(led)->setLabel("Y");
    c.connect({sw, 0}, {orGate, 1});
    c.connect({inverter, 0}, {orGate, 2});
    c.connect({orGate, 0}, {inverter, 1});   // loop that cannot settle
    c.connect({orGate, 0}, {led, 0});

    const TruthTableResult result = TruthTableGenerator::generate(c, {sw}, {led});
    EXPECT_EQ(result.status, TruthTableStatus::Oscillating);
    EXPECT_TRUE(result.hasTable());
    ASSERT_EQ(result.table.rowCount(), 2u);
    EXPECT_FALSE(result.message.empty());
}

TEST(TruthTableTest, TwelveInputsIsAllowedAndFinishes)
{
    Circuit c;
    const ComponentId gate = c.add(std::make_unique<AndGate>());
    c.setInputCount(gate, 8);
    std::vector<ComponentId> switches;
    for (int i = 0; i < 12; ++i) {
        const ComponentId sw = c.add(std::make_unique<Switch>());
        switches.push_back(sw);
        if (i < 8)
            c.connect({sw, 0}, {gate, i + 1});
    }
    const ComponentId led = c.add(std::make_unique<Led>());
    c.connect({gate, 0}, {led, 0});

    const TruthTableResult result = TruthTableGenerator::generate(c, switches, {led});
    ASSERT_EQ(result.status, TruthTableStatus::Ok);
    EXPECT_EQ(result.table.rowCount(), 4096u);
    // Only the last row has all eight wired inputs high.
    EXPECT_EQ(result.table.outputs[4095].front(), Signal::High);
    EXPECT_EQ(result.table.outputs[0].front(), Signal::Low);
}

TEST(TruthTableTest, CsvExportHasAHeaderAndOneLinePerRow)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    const TruthTableResult result =
        TruthTableGenerator::generate(c, {h.a, h.b}, {h.sumLed, h.carryLed});
    const std::string csv = result.table.toCsv();

    EXPECT_EQ(csv.substr(0, csv.find('\n')), "A,B,S,C");
    EXPECT_EQ(std::count(csv.begin(), csv.end(), '\n'), 5);   // header + 4 rows
    EXPECT_NE(csv.find("1,1,0,1"), std::string::npos);
}

TEST(TruthTableTest, UnlabelledPortsFallBackToTypeAndId)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId led = c.add(std::make_unique<Led>());
    c.connect({sw, 0}, {led, 0});

    const TruthTableResult result = TruthTableGenerator::generate(c, {sw}, {led});
    ASSERT_EQ(result.status, TruthTableStatus::Ok);
    EXPECT_EQ(result.table.inputNames.front(), "SWITCH " + std::to_string(sw));
    EXPECT_EQ(result.table.outputs[1].front(), Signal::High);
}

TEST(TruthTableTest, DefaultSelectionListsEverySwitchAndLed)
{
    Circuit c;
    lbtest::buildHalfAdder(c);
    EXPECT_EQ(TruthTableGenerator::allSwitches(c).size(), 2u);
    EXPECT_EQ(TruthTableGenerator::allLeds(c).size(), 2u);
}
