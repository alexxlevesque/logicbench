// The example circuits are documentation that runs, so they are tested like
// any other code: if a change to the format or to the engine breaks one, this
// fails before a student opens it.

#include <gtest/gtest.h>

#include "logicbench/CircuitSerializer.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Simulator.h"
#include "logicbench/TruthTable.h"

#include <string>

using namespace lb;

namespace {

std::string examplePath(const std::string& file)
{
    return std::string(LOGICBENCH_EXAMPLES_DIR) + "/" + file;
}

std::unique_ptr<Circuit> loadExample(const std::string& file)
{
    return CircuitSerializer::load(examplePath(file));
}

Component* byLabel(const Circuit& circuit, const std::string& label)
{
    for (const std::unique_ptr<Component>& c : circuit.components()) {
        if (c->label() == label)
            return c.get();
    }
    return nullptr;
}

void setSwitchByLabel(Circuit& circuit, Simulator& sim, const std::string& label, bool high)
{
    auto* toggle = dynamic_cast<Switch*>(byLabel(circuit, label));
    ASSERT_NE(toggle, nullptr) << "no switch labelled " << label;
    toggle->setState(signalFromBool(high));
    sim.evaluateFrom(toggle->id());
}

} // namespace

TEST(ExamplesTest, EveryExampleOpens)
{
    for (const char* file : {"half_adder.lbc", "full_adder.lbc", "adder4.lbc",
                             "counter3.lbc", "sr_latch.lbc", "ring_oscillator.lbc"}) {
        EXPECT_NO_THROW({
            std::unique_ptr<Circuit> circuit = loadExample(file);
            EXPECT_GT(circuit->componentCount(), 0u) << file;
        }) << file;
    }
}

TEST(ExamplesTest, HalfAdderMatchesItsTruthTable)
{
    std::unique_ptr<Circuit> circuit = loadExample("half_adder.lbc");
    const TruthTableResult result = TruthTableGenerator::generate(
        *circuit,
        {byLabel(*circuit, "A")->id(), byLabel(*circuit, "B")->id()},
        {byLabel(*circuit, "S")->id(), byLabel(*circuit, "C")->id()});

    ASSERT_EQ(result.status, TruthTableStatus::Ok);
    ASSERT_EQ(result.table.rowCount(), 4u);
    EXPECT_EQ(result.table.toCsv(),
              "A,B,S,C\n"
              "0,0,0,0\n"
              "0,1,1,0\n"
              "1,0,1,0\n"
              "1,1,0,1\n");
}

TEST(ExamplesTest, FullAdderAddsThreeBits)
{
    std::unique_ptr<Circuit> circuit = loadExample("full_adder.lbc");
    Simulator sim(*circuit);
    sim.reset();

    for (int value = 0; value < 8; ++value) {
        const bool a = value & 4;
        const bool b = value & 2;
        const bool cin = value & 1;
        setSwitchByLabel(*circuit, sim, "A", a);
        setSwitchByLabel(*circuit, sim, "B", b);
        setSwitchByLabel(*circuit, sim, "Cin", cin);

        const int total = static_cast<int>(a) + static_cast<int>(b) + static_cast<int>(cin);
        EXPECT_EQ(byLabel(*circuit, "S")->pin(0).signal(), signalFromBool(total % 2 == 1))
            << "A=" << a << " B=" << b << " Cin=" << cin;
        EXPECT_EQ(byLabel(*circuit, "Cout")->pin(0).signal(), signalFromBool(total >= 2))
            << "A=" << a << " B=" << b << " Cin=" << cin;
    }
}

TEST(ExamplesTest, FourBitAdderAddsEveryPairOfNibbles)
{
    // Four full-adder blocks, each holding two half-adder blocks: this is the
    // deepest nesting the examples use, and the arithmetic is the proof that
    // subcircuits compose.
    std::unique_ptr<Circuit> circuit = loadExample("adder4.lbc");
    Simulator sim(*circuit);
    sim.reset();

    for (int a = 0; a < 16; ++a) {
        for (int b = 0; b < 16; ++b) {
            for (int bit = 0; bit < 4; ++bit) {
                setSwitchByLabel(*circuit, sim, "A" + std::to_string(bit), (a >> bit) & 1);
                setSwitchByLabel(*circuit, sim, "B" + std::to_string(bit), (b >> bit) & 1);
            }
            setSwitchByLabel(*circuit, sim, "Cin", false);

            int sum = 0;
            for (int bit = 0; bit < 4; ++bit) {
                if (byLabel(*circuit, "S" + std::to_string(bit))->pin(0).signal() == Signal::High)
                    sum |= (1 << bit);
            }
            if (byLabel(*circuit, "Cout")->pin(0).signal() == Signal::High)
                sum |= 16;

            EXPECT_EQ(sum, a + b) << a << " + " << b;
        }
    }
}

TEST(ExamplesTest, CounterCountsUpInBinary)
{
    std::unique_ptr<Circuit> circuit = loadExample("counter3.lbc");
    Simulator sim(*circuit);
    sim.reset();

    const auto readCount = [&circuit] {
        int value = 0;
        for (int bit = 0; bit < 3; ++bit) {
            if (byLabel(*circuit, "bit " + std::to_string(bit))->pin(0).signal() == Signal::High)
                value |= (1 << bit);
        }
        return value;
    };

    // The clock has a period of 8 ticks, so one count per 8 ticks.
    std::vector<int> observed;
    for (int step = 0; step < 8; ++step) {
        sim.advance(8);
        observed.push_back(readCount());
    }

    // Consecutive samples must increase by one, wrapping at 8.
    for (std::size_t i = 1; i < observed.size(); ++i)
        EXPECT_EQ(observed[i], (observed[i - 1] + 1) % 8) << "sample " << i;
}

TEST(ExamplesTest, SrLatchHoldsItsStateAfterTheInputGoesLow)
{
    std::unique_ptr<Circuit> circuit = loadExample("sr_latch.lbc");
    Simulator sim(*circuit);
    sim.reset();

    setSwitchByLabel(*circuit, sim, "S", true);
    EXPECT_EQ(byLabel(*circuit, "Q")->pin(0).signal(), Signal::High);

    setSwitchByLabel(*circuit, sim, "S", false);
    EXPECT_EQ(byLabel(*circuit, "Q")->pin(0).signal(), Signal::High) << "the latch should hold";

    setSwitchByLabel(*circuit, sim, "R", true);
    EXPECT_EQ(byLabel(*circuit, "Q")->pin(0).signal(), Signal::Low);
    setSwitchByLabel(*circuit, sim, "R", false);
    EXPECT_EQ(byLabel(*circuit, "Q")->pin(0).signal(), Signal::Low);
}

TEST(ExamplesTest, RingOscillatorIsFlaggedRatherThanHanging)
{
    std::unique_ptr<Circuit> circuit = loadExample("ring_oscillator.lbc");
    Simulator sim(*circuit);
    const SettleReport report = sim.reset();

    EXPECT_FALSE(report.stable());
    EXPECT_EQ(report.oscillating.size(), 3u);
    EXPECT_EQ(byLabel(*circuit, "out")->pin(0).signal(), Signal::Undefined);
}
