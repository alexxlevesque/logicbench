#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/DFlipFlop.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Simulator.h"

#include <cstdlib>

using namespace lb;
using lbtest::ledValue;
using lbtest::setSwitch;

TEST(SimulatorTest, HalfAdderProducesTheRightSumAndCarry)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    Simulator sim(c);
    sim.reset();

    struct Row { bool a, b; Signal sum, carry; };
    const Row rows[] = {
        {false, false, Signal::Low,  Signal::Low},
        {false, true,  Signal::High, Signal::Low},
        {true,  false, Signal::High, Signal::Low},
        {true,  true,  Signal::Low,  Signal::High},
    };

    for (const Row& row : rows) {
        setSwitch(c, sim, h.a, row.a);
        setSwitch(c, sim, h.b, row.b);
        EXPECT_EQ(ledValue(c, h.sumLed), row.sum) << "A=" << row.a << " B=" << row.b;
        EXPECT_EQ(ledValue(c, h.carryLed), row.carry) << "A=" << row.a << " B=" << row.b;
    }
}

TEST(SimulatorTest, FloatingInputShowsAsUndefinedNotLow)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId gate = c.add(std::make_unique<AndGate>());
    const ComponentId led = c.add(std::make_unique<Led>());
    c.connect({sw, 0}, {gate, 1});      // pin 2 of the gate is left unwired
    c.connect({gate, 0}, {led, 0});

    Simulator sim(c);
    sim.reset();
    setSwitch(c, sim, sw, true);
    EXPECT_EQ(ledValue(c, led), Signal::Undefined);

    // Driving the known input low settles the output regardless of the
    // floating one.
    setSwitch(c, sim, sw, false);
    EXPECT_EQ(ledValue(c, led), Signal::Low);
}

TEST(SimulatorTest, OnlyTheAffectedPartOfTheCircuitIsReEvaluated)
{
    // Two half adders sharing nothing: toggling one must not wake the other.
    Circuit c;
    const lbtest::HalfAdder left = lbtest::buildHalfAdder(c, "L");
    lbtest::buildHalfAdder(c, "R");
    Simulator sim(c);
    sim.reset();

    auto* sw = dynamic_cast<Switch*>(c.component(left.a));
    sw->setState(Signal::High);
    const SettleReport report = sim.evaluateFrom(left.a);

    EXPECT_TRUE(report.stable());
    // Switch, XOR, AND, two LEDs — the right-hand adder is untouched.
    EXPECT_LE(report.eventsProcessed, 6u);
}

TEST(SimulatorTest, OscillatingLoopIsFlaggedInsteadOfHanging)
{
    // A NOT gate wired to itself can never settle. The simulator has to stop,
    // report it, and leave the signal Undefined.
    Circuit c;
    const ComponentId inverter = c.add(std::make_unique<NotGate>());
    const ComponentId led = c.add(std::make_unique<Led>());
    c.connect({inverter, 0}, {inverter, 1});
    c.connect({inverter, 0}, {led, 0});

    Simulator sim(c);
    const SettleReport report = sim.reset();

    EXPECT_FALSE(report.stable());
    ASSERT_EQ(report.oscillating.size(), 1u);
    EXPECT_EQ(report.oscillating.front(), inverter);
    EXPECT_EQ(ledValue(c, led), Signal::Undefined);
    EXPECT_LT(report.eventsProcessed, Simulator::kDefaultEventCap);
}

TEST(SimulatorTest, ThreeInverterRingIsAlsoCaught)
{
    Circuit c;
    const ComponentId a = c.add(std::make_unique<NotGate>());
    const ComponentId b = c.add(std::make_unique<NotGate>());
    const ComponentId d = c.add(std::make_unique<NotGate>());
    c.connect({a, 0}, {b, 1});
    c.connect({b, 0}, {d, 1});
    c.connect({d, 0}, {a, 1});

    Simulator sim(c);
    const SettleReport report = sim.reset();
    EXPECT_FALSE(report.stable());
    EXPECT_FALSE(report.oscillating.empty());
}

TEST(SimulatorTest, StableFeedbackThroughAFlipFlopIsNotAnOscillation)
{
    // Q-bar back into D is a toggle, not a runaway loop: the flip-flop only
    // changes on a clock edge.
    Circuit c;
    const ComponentId clock = c.add(std::make_unique<Clock>());
    const ComponentId dff = c.add(std::make_unique<DFlipFlop>());
    const ComponentId led = c.add(std::make_unique<Led>());
    dynamic_cast<Clock*>(c.component(clock))->setPeriod(4);
    c.connect({clock, 0}, {dff, 3});
    c.connect({dff, 1}, {dff, 2});      // QBAR -> D
    c.connect({dff, 0}, {led, 0});      // Q -> LED

    Simulator sim(c);
    sim.reset();
    const SettleReport report = sim.advance(16);
    EXPECT_TRUE(report.stable());
}

TEST(SimulatorTest, ClockDrivenFlipFlopDividesTheClockByTwo)
{
    Circuit c;
    const ComponentId clock = c.add(std::make_unique<Clock>());
    const ComponentId dff = c.add(std::make_unique<DFlipFlop>());
    dynamic_cast<Clock*>(c.component(clock))->setPeriod(4);   // flips every 2 ticks
    c.connect({clock, 0}, {dff, 3});
    c.connect({dff, 1}, {dff, 2});

    Simulator sim(c);
    sim.reset();

    std::vector<Signal> clockSamples;
    std::vector<Signal> qSamples;
    sim.advance(16, [&](Tick) {
        clockSamples.push_back(c.component(clock)->pin(0).signal());
        qSamples.push_back(c.component(dff)->pin(0).signal());
    });

    const auto edges = [](const std::vector<Signal>& samples) {
        int count = 0;
        for (std::size_t i = 1; i < samples.size(); ++i) {
            if (samples[i - 1] == Signal::Low && samples[i] == Signal::High)
                ++count;
        }
        return count;
    };
    const int clockEdges = edges(clockSamples);
    const int qEdges = edges(qSamples);
    EXPECT_GE(clockEdges, 2);
    // Q rises once per two clock rises. Counting edges in a finite window is
    // off by at most one at either end: a rise on the first sample has no
    // preceding low to be seen against, and a clock rise near the last sample
    // may not have produced its Q edge yet.
    EXPECT_LE(std::abs(qEdges * 2 - clockEdges), 1)
        << "clock rises: " << clockEdges << ", Q rises: " << qEdges;
}

TEST(SimulatorTest, ResetClearsFlipFlopStateButKeepsSwitchPositions)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    Simulator sim(c);
    sim.reset();
    setSwitch(c, sim, h.a, true);
    setSwitch(c, sim, h.b, true);
    ASSERT_EQ(ledValue(c, h.carryLed), Signal::High);

    const Tick beforeReset = sim.now();
    sim.reset();
    // Time restarts at zero and then advances only by the depth of the
    // circuit as the reset settles — one gate delay plus the LED.
    EXPECT_LT(sim.now(), beforeReset);
    EXPECT_LE(sim.now(), 2u);
    // The switches the user set stay set, and the circuit settles back to the
    // same answer rather than to zero.
    EXPECT_EQ(ledValue(c, h.carryLed), Signal::High);
}

TEST(SimulatorTest, ResolvedSrLatchIsNotReportedAsAnOscillation)
{
    // Cross-coupled NOR gates are a legitimate feedback loop, and a student
    // in a digital-logic course will build one. Once set, it holds definite
    // levels, and the loop detector must leave it alone.
    Circuit c;
    const ComponentId set = c.add(std::make_unique<Switch>());
    const ComponentId reset = c.add(std::make_unique<Switch>());
    const ComponentId norQ = c.add(std::make_unique<NorGate>());
    const ComponentId norQbar = c.add(std::make_unique<NorGate>());
    c.connect({reset, 0}, {norQ, 1});
    c.connect({norQbar, 0}, {norQ, 2});
    c.connect({set, 0}, {norQbar, 1});
    c.connect({norQ, 0}, {norQbar, 2});

    Simulator sim(c);
    sim.reset();

    // S = 1, R = 0 drives the latch to a definite state.
    dynamic_cast<Switch*>(c.component(set))->setState(Signal::High);
    const SettleReport report = sim.evaluateFrom(set);

    EXPECT_TRUE(report.stable()) << "a latch that has resolved is not an oscillation";
    // Q = NOR(R, Qbar) and Qbar = NOR(S, Q): asserting S drives Qbar low,
    // which releases Q high, and the pair then holds.
    EXPECT_EQ(c.component(norQ)->pin(0).signal(), Signal::High);
    EXPECT_EQ(c.component(norQbar)->pin(0).signal(), Signal::Low);
}

TEST(SimulatorTest, UnsetSrLatchIsReportedAsUndefined)
{
    // With both inputs low from power-up the same latch has no defined state.
    // Reporting that honestly is the point of the third signal value.
    Circuit c;
    const ComponentId norQ = c.add(std::make_unique<NorGate>());
    const ComponentId norQbar = c.add(std::make_unique<NorGate>());
    const ComponentId low1 = c.add(std::make_unique<Switch>());
    const ComponentId low2 = c.add(std::make_unique<Switch>());
    c.connect({low1, 0}, {norQ, 1});
    c.connect({norQbar, 0}, {norQ, 2});
    c.connect({low2, 0}, {norQbar, 1});
    c.connect({norQ, 0}, {norQbar, 2});

    Simulator sim(c);
    const SettleReport report = sim.reset();
    EXPECT_FALSE(report.stable());
    EXPECT_EQ(report.oscillating.size(), 2u);
}

TEST(SimulatorTest, ALoopCarryingDefiniteLevelsIsStoppedByTheEvaluationBudget)
{
    // Closing a loop around a gate that already holds a definite level makes
    // it flip on every pass. The budget is what stops the app from hanging.
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId inverter = c.add(std::make_unique<NotGate>());
    c.connect({sw, 0}, {inverter, 1});

    Simulator sim(c);
    sim.reset();
    dynamic_cast<Switch*>(c.component(sw))->setState(Signal::High);
    sim.evaluateFrom(sw);
    ASSERT_EQ(c.component(inverter)->pin(0).signal(), Signal::Low);

    // Rewire the inverter's input from the switch to its own output.
    c.disconnect(c.wires().front().id);
    c.connect({inverter, 0}, {inverter, 1});

    const SettleReport report = sim.evaluateFrom(inverter);
    EXPECT_FALSE(report.stable());
    EXPECT_EQ(report.oscillating.size(), 1u);
    EXPECT_LT(report.eventsProcessed, Simulator::kDefaultEventCap);
}

TEST(SimulatorTest, DelaysOrderPropagationInTime)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId slow = c.add(std::make_unique<NotGate>());
    const ComponentId led = c.add(std::make_unique<Led>());
    c.component(slow)->setDelay(5);
    c.connect({sw, 0}, {slow, 1});
    c.connect({slow, 0}, {led, 0});

    Simulator sim(c);
    sim.reset();
    const Tick before = sim.now();
    setSwitch(c, sim, sw, true);
    EXPECT_GE(sim.now(), before + 5) << "the slow gate should advance virtual time";
    EXPECT_EQ(ledValue(c, led), Signal::Low);
}

TEST(SimulatorTest, StoppedClockDoesNotAdvance)
{
    Circuit c;
    const ComponentId clock = c.add(std::make_unique<Clock>());
    auto* clk = dynamic_cast<Clock*>(c.component(clock));
    clk->setPeriod(4);
    clk->setRunning(false);

    Simulator sim(c);
    sim.reset();
    const Signal before = c.component(clock)->pin(0).signal();
    sim.advance(20);
    EXPECT_EQ(c.component(clock)->pin(0).signal(), before);
}
