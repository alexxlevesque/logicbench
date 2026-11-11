#include <gtest/gtest.h>

#include "logicbench/Circuit.h"
#include "logicbench/CircuitSerializer.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Multiplexer.h"
#include "logicbench/Simulator.h"

#include <memory>

using namespace lb;

namespace {

constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;

Signal apply(Multiplexer& mux, std::initializer_list<Signal> data, std::initializer_list<Signal> select)
{
    std::size_t index = 1;
    for (Signal s : data)
        mux.pin(index++).setSignal(s);
    for (Signal s : select)
        mux.pin(index++).setSignal(s);
    mux.evaluate();
    return mux.pin(0).signal();
}

} // namespace

TEST(MultiplexerTest, PinLayout)
{
    Multiplexer two(2);
    EXPECT_EQ(two.typeName(), "MUX2");
    EXPECT_EQ(two.pinCount(), 4u);
    EXPECT_EQ(two.pinIndex("Y"), 0);
    EXPECT_EQ(two.pinIndex("D1"), 2);
    EXPECT_EQ(two.pinIndex("S0"), 3);

    Multiplexer four(4);
    EXPECT_EQ(four.typeName(), "MUX4");
    EXPECT_EQ(four.pinCount(), 7u);
    EXPECT_EQ(four.pinIndex("D3"), 4);
    EXPECT_EQ(four.pinIndex("S1"), 6);
}

TEST(MultiplexerTest, TwoToOneFullTruthTable)
{
    Multiplexer mux(2);
    for (Signal d0 : {L, H, X}) {
        for (Signal d1 : {L, H, X}) {
            EXPECT_EQ(apply(mux, {d0, d1}, {L}), d0);
            EXPECT_EQ(apply(mux, {d0, d1}, {H}), d1);
            // Select unknown: the output is certain only when both inputs agree.
            const Signal expected = (d0 == d1) ? d0 : X;
            EXPECT_EQ(apply(mux, {d0, d1}, {X}), expected) << d0 << " " << d1;
        }
    }
}

TEST(MultiplexerTest, FourToOneSelectsEveryInput)
{
    Multiplexer mux(4);
    const Signal bits[] = {L, H};
    for (int chosen = 0; chosen < 4; ++chosen) {
        // Put a 1 on the chosen input and 0 on the others.
        std::vector<Signal> data(4, L);
        data[static_cast<std::size_t>(chosen)] = H;
        for (std::size_t i = 0; i < 4; ++i)
            mux.pin(1 + i).setSignal(data[i]);
        mux.pin(5).setSignal(bits[chosen & 1]);          // S0 is the low bit
        mux.pin(6).setSignal(bits[(chosen >> 1) & 1]);
        mux.evaluate();
        EXPECT_EQ(mux.pin(0).signal(), H) << "select " << chosen;
    }
}

TEST(MultiplexerTest, FourToOneWithAnUnknownSelectBit)
{
    Multiplexer mux(4);
    // S1 = 0 known, S0 unknown: D0 or D1.
    EXPECT_EQ(apply(mux, {H, H, L, L}, {X, L}), H);
    EXPECT_EQ(apply(mux, {H, L, H, H}, {X, L}), X);
    // S0 = 1 known, S1 unknown: D1 or D3.
    EXPECT_EQ(apply(mux, {L, H, L, H}, {H, X}), H);
    // Both unknown: all four must agree.
    EXPECT_EQ(apply(mux, {L, L, L, L}, {X, X}), L);
    EXPECT_EQ(apply(mux, {L, L, L, H}, {X, X}), X);
}

TEST(MultiplexerTest, WorksInACircuit)
{
    Circuit c;
    auto d0 = std::make_unique<Switch>();
    auto d1 = std::make_unique<Switch>();
    auto s = std::make_unique<Switch>();
    d1->setState(H);
    s->setState(H);
    const ComponentId a = c.add(std::move(d0));
    const ComponentId b = c.add(std::move(d1));
    const ComponentId sel = c.add(std::move(s));
    const ComponentId mux = c.add(std::make_unique<Multiplexer>(2));
    const ComponentId led = c.add(std::make_unique<Led>());
    c.connect({a, 0}, {mux, 1});
    c.connect({b, 0}, {mux, 2});
    c.connect({sel, 0}, {mux, 3});
    c.connect({mux, 0}, {led, 0});

    Simulator sim(c);
    sim.reset();
    EXPECT_EQ(c.component(led)->pin(0).signal(), H);
}

TEST(MultiplexerTest, BothSizesAreRegisteredAndSurviveSaveAndReopen)
{
    for (const char* type : {"MUX2", "MUX4"}) {
        Circuit c;
        std::unique_ptr<Component> part = ComponentFactory::standard().create(type);
        part->setLabel("M");
        const ComponentId id = c.add(std::move(part));
        std::unique_ptr<Circuit> reopened = CircuitSerializer::fromJson(CircuitSerializer::toJson(c));
        ASSERT_NE(reopened->component(id), nullptr);
        EXPECT_EQ(reopened->component(id)->typeName(), type);
        EXPECT_EQ(reopened->component(id)->pinCount(), c.component(id)->pinCount());
        EXPECT_EQ(reopened->component(id)->label(), "M");
    }
}
