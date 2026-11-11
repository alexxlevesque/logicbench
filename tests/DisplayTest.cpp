#include <gtest/gtest.h>

#include "logicbench/Circuit.h"
#include "logicbench/CircuitSerializer.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/Decoder.h"
#include "logicbench/Displays.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Simulator.h"

#include <memory>

using namespace lb;

namespace {
constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;
} // namespace

TEST(DisplayTest, HexDisplayShowsEveryDigit)
{
    HexDisplay hex;
    EXPECT_EQ(hex.inputPins().size(), 4u);
    EXPECT_TRUE(hex.outputPins().empty());
    for (int n = 0; n < 16; ++n) {
        for (int bit = 0; bit < 4; ++bit)
            hex.pin(static_cast<std::size_t>(bit)).setSignal(signalFromBool((n >> bit) & 1));
        EXPECT_EQ(hex.digit(), n);
    }
}

TEST(DisplayTest, HexDisplayDigitIsMinusOneWhenAnyInputIsX)
{
    HexDisplay hex;
    EXPECT_EQ(hex.digit(), -1);                    // fresh: every input floating
    for (int bit = 0; bit < 4; ++bit) {
        for (int i = 0; i < 4; ++i)
            hex.pin(static_cast<std::size_t>(i)).setSignal(H);
        hex.pin(static_cast<std::size_t>(bit)).setSignal(X);
        EXPECT_EQ(hex.digit(), -1) << "X on D" << bit;
    }
}

TEST(DisplayTest, SevenSegmentReadsEachSegmentIncludingX)
{
    SevenSegmentDisplay seg;
    ASSERT_EQ(seg.inputPins().size(), 7u);
    EXPECT_EQ(seg.pinIndex("a"), 0);
    EXPECT_EQ(seg.pinIndex("g"), 6);
    const Signal values[] = {H, L, X, H, L, X, H};
    for (int i = 0; i < 7; ++i)
        seg.pin(static_cast<std::size_t>(i)).setSignal(values[i]);
    for (int i = 0; i < 7; ++i)
        EXPECT_EQ(seg.segment(i), values[i]);
    EXPECT_EQ(seg.segment(7), X);                  // out of range
}

TEST(DisplayTest, DisplaysDriveNothing)
{
    HexDisplay hex;
    SevenSegmentDisplay seg;
    EXPECT_FALSE(hex.evaluate());
    EXPECT_FALSE(seg.evaluate());
}

TEST(DisplayTest, HexDisplayFollowsSwitchesInACircuit)
{
    Circuit c;
    ComponentId switches[4];
    for (int i = 0; i < 4; ++i)
        switches[i] = c.add(std::make_unique<Switch>());
    const ComponentId hex = c.add(std::make_unique<HexDisplay>());
    for (int i = 0; i < 4; ++i)
        c.connect({switches[i], 0}, {hex, i});

    Simulator sim(c);
    sim.reset();
    auto* display = static_cast<HexDisplay*>(c.component(hex));
    EXPECT_EQ(display->digit(), 0);

    static_cast<Switch*>(c.component(switches[0]))->setState(H);   // 1
    static_cast<Switch*>(c.component(switches[3]))->setState(H);   // + 8
    sim.evaluateFrom(switches[0]);
    sim.evaluateFrom(switches[3]);
    EXPECT_EQ(display->digit(), 9);
}

TEST(DisplayTest, EveryDisplayIsRegisteredAndSurvivesSaveAndReopen)
{
    for (const char* type : {"HEX", "SEG7"}) {
        Circuit c;
        std::unique_ptr<Component> part = ComponentFactory::standard().create(type);
        part->setLabel("DISP");
        part->setPosition(100, 40);
        const ComponentId id = c.add(std::move(part));
        std::unique_ptr<Circuit> reopened = CircuitSerializer::fromJson(CircuitSerializer::toJson(c));
        ASSERT_NE(reopened->component(id), nullptr);
        EXPECT_EQ(reopened->component(id)->typeName(), type);
        EXPECT_EQ(reopened->component(id)->label(), "DISP");
        EXPECT_EQ(reopened->component(id)->pinCount(), c.component(id)->pinCount());
    }
}
