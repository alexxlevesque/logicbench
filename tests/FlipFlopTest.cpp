#include <gtest/gtest.h>

#include "logicbench/Circuit.h"
#include "logicbench/CircuitSerializer.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/FlipFlop.h"

#include <memory>

using namespace lb;

namespace {

constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;

/// Sets the data inputs, then gives the flip-flop one full clock pulse
/// (low, then high) and returns Q.
Signal clockIn(FlipFlop& ff, std::initializer_list<Signal> data)
{
    std::size_t index = 2;
    for (Signal s : data)
        ff.pin(index++).setSignal(s);
    ff.pin(static_cast<std::size_t>(ff.clockPin())).setSignal(L);
    ff.evaluate();
    ff.pin(static_cast<std::size_t>(ff.clockPin())).setSignal(H);
    ff.evaluate();
    return ff.pin(0).signal();
}

/// Puts the flip-flop into a known state first.
void preset(FlipFlop& ff, Signal q)
{
    ff.setProperty("initial", q == H ? "1" : "0");
    ff.reset();
}

} // namespace

TEST(FlipFlopTest, PinLayoutKeepsQAndQbarFirstAndClockLast)
{
    DFlipFlop d;
    JKFlipFlop jk;
    TFlipFlop t;
    EXPECT_EQ(d.pinIndex("Q"), 0);
    EXPECT_EQ(d.pinIndex("QBAR"), 1);
    EXPECT_EQ(d.pinIndex("D"), 2);
    EXPECT_EQ(d.pinIndex("CLK"), 3);          // unchanged from the prototype's DFF
    EXPECT_EQ(jk.pinIndex("J"), 2);
    EXPECT_EQ(jk.pinIndex("K"), 3);
    EXPECT_EQ(jk.clockPin(), 4);
    EXPECT_EQ(t.pinIndex("T"), 2);
    EXPECT_EQ(t.clockPin(), 3);
}

TEST(FlipFlopTest, DFullTruthTableIncludingX)
{
    for (Signal q : {L, H}) {
        for (Signal d : {L, H, X}) {
            DFlipFlop ff;
            preset(ff, q);
            EXPECT_EQ(clockIn(ff, {d}), d) << "Q=" << q << " D=" << d;
        }
    }
}

TEST(FlipFlopTest, JKFullTruthTableIncludingX)
{
    struct Row { Signal q, j, k, next; };
    const Row rows[] = {
        {L, L, L, L}, {H, L, L, H},           // hold
        {L, H, L, H}, {H, H, L, H},           // set
        {L, L, H, L}, {H, L, H, L},           // reset
        {L, H, H, H}, {H, H, H, L},           // toggle
        {L, X, L, X}, {H, X, L, H},           // J unknown: only a held 1 is certain
        {L, L, X, L}, {H, L, X, X},           // K unknown: only a held 0 is certain
        {L, X, X, X}, {H, X, X, X},
        {L, X, H, X}, {H, H, X, X},           // toggle-or-reset, set-or-toggle: unknown
    };
    for (const Row& r : rows) {
        JKFlipFlop ff;
        preset(ff, r.q);
        EXPECT_EQ(clockIn(ff, {r.j, r.k}), r.next) << "Q=" << r.q << " J=" << r.j << " K=" << r.k;
    }
}

TEST(FlipFlopTest, TFullTruthTableIncludingX)
{
    struct Row { Signal q, t, next; };
    const Row rows[] = {{L, L, L}, {H, L, H}, {L, H, H}, {H, H, L}, {L, X, X}, {H, X, X}};
    for (const Row& r : rows) {
        TFlipFlop ff;
        preset(ff, r.q);
        EXPECT_EQ(clockIn(ff, {r.t}), r.next) << "Q=" << r.q << " T=" << r.t;
    }
}

TEST(FlipFlopTest, QbarIsTheComplementOfQ)
{
    JKFlipFlop ff;
    preset(ff, L);
    clockIn(ff, {H, L});
    EXPECT_EQ(ff.pin(0).signal(), H);
    EXPECT_EQ(ff.pin(1).signal(), L);
}

TEST(FlipFlopTest, ChangesOnlyOnARisingEdge)
{
    TFlipFlop ff;
    preset(ff, L);
    ff.pin(2).setSignal(H);                   // T = 1: would toggle on every edge

    const std::size_t clk = static_cast<std::size_t>(ff.clockPin());
    ff.pin(clk).setSignal(H);                 // first sight of the clock: no edge yet
    ff.evaluate();
    EXPECT_EQ(ff.state(), L);

    ff.pin(clk).setSignal(L);                 // falling edge: no change
    ff.evaluate();
    EXPECT_EQ(ff.state(), L);

    ff.pin(clk).setSignal(H);                 // rising edge: toggles
    ff.evaluate();
    EXPECT_EQ(ff.state(), H);

    ff.evaluate();                            // clock held high: no second toggle
    EXPECT_EQ(ff.state(), H);

    ff.pin(clk).setSignal(X);                 // X to high is not a clean edge
    ff.evaluate();
    ff.pin(clk).setSignal(H);
    ff.evaluate();
    EXPECT_EQ(ff.state(), H);
}

TEST(FlipFlopTest, ResetReturnsToTheInitialState)
{
    DFlipFlop ff;
    ff.setProperty("initial", "1");
    clockIn(ff, {L});
    EXPECT_EQ(ff.state(), L);
    ff.reset();
    EXPECT_EQ(ff.state(), H);
}

TEST(FlipFlopTest, AllFlipFlopsAreSequential)
{
    EXPECT_TRUE(DFlipFlop().isSequential());
    EXPECT_TRUE(JKFlipFlop().isSequential());
    EXPECT_TRUE(TFlipFlop().isSequential());
}

TEST(FlipFlopTest, CloneHasIndependentState)
{
    TFlipFlop original;
    preset(original, L);
    std::unique_ptr<Component> copy = original.clone();
    ASSERT_EQ(copy->typeName(), "TFF");
    clockIn(original, {H});
    EXPECT_EQ(original.state(), H);
    EXPECT_EQ(static_cast<TFlipFlop&>(*copy).state(), L);
}

TEST(FlipFlopTest, EveryNewTypeIsRegisteredAndSurvivesSaveAndReopen)
{
    for (const char* type : {"DFF", "JKFF", "TFF"}) {
        Circuit c;
        std::unique_ptr<Component> part = ComponentFactory::standard().create(type);
        part->setLabel("U1");
        part->setProperty("initial", "1");
        part->setPosition(40, 60);
        const ComponentId id = c.add(std::move(part));

        std::unique_ptr<Circuit> reopened = CircuitSerializer::fromJson(CircuitSerializer::toJson(c));
        Component* back = reopened->component(id);
        ASSERT_NE(back, nullptr) << type;
        EXPECT_EQ(back->typeName(), type);
        EXPECT_EQ(back->label(), "U1");
        EXPECT_EQ(back->pinCount(), c.component(id)->pinCount());
        EXPECT_EQ(back->properties().at("initial"), "1");
    }
}
