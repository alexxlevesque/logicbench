#include <gtest/gtest.h>

#include "logicbench/Circuit.h"
#include "logicbench/CircuitSerializer.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/Decoder.h"

#include <memory>
#include <vector>

using namespace lb;

namespace {

constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;

/// Sets the address (A0 first) and returns every output.
std::vector<Signal> apply(Decoder& dec, const std::vector<Signal>& address)
{
    for (std::size_t bit = 0; bit < address.size(); ++bit)
        dec.pin(static_cast<std::size_t>(dec.outputCount()) + bit).setSignal(address[bit]);
    dec.evaluate();
    std::vector<Signal> out;
    for (int i = 0; i < dec.outputCount(); ++i)
        out.push_back(dec.pin(static_cast<std::size_t>(i)).signal());
    return out;
}

} // namespace

TEST(DecoderTest, PinLayout)
{
    Decoder two(2);
    EXPECT_EQ(two.typeName(), "DEC2");
    EXPECT_EQ(two.outputPins().size(), 4u);
    EXPECT_EQ(two.inputPins().size(), 2u);
    EXPECT_EQ(two.pinIndex("Y3"), 3);
    EXPECT_EQ(two.pinIndex("A0"), 4);

    Decoder three(3);
    EXPECT_EQ(three.typeName(), "DEC3");
    EXPECT_EQ(three.outputPins().size(), 8u);
    EXPECT_EQ(three.pinIndex("A2"), 10);
}

TEST(DecoderTest, TwoToFourFullTruthTable)
{
    Decoder dec(2);
    for (int n = 0; n < 4; ++n) {
        const std::vector<Signal> out = apply(dec, {signalFromBool(n & 1), signalFromBool((n >> 1) & 1)});
        for (int i = 0; i < 4; ++i)
            EXPECT_EQ(out[static_cast<std::size_t>(i)], i == n ? H : L) << "address " << n << " output " << i;
    }
}

TEST(DecoderTest, ThreeToEightFullTruthTable)
{
    Decoder dec(3);
    for (int n = 0; n < 8; ++n) {
        const std::vector<Signal> out = apply(
            dec, {signalFromBool(n & 1), signalFromBool((n >> 1) & 1), signalFromBool((n >> 2) & 1)});
        for (int i = 0; i < 8; ++i)
            EXPECT_EQ(out[static_cast<std::size_t>(i)], i == n ? H : L) << "address " << n << " output " << i;
    }
}

TEST(DecoderTest, UnknownAddressBitsGiveXOnlyWhereTheOutputMightBeChosen)
{
    Decoder dec(2);
    // A0 unknown, A1 = 1: Y2 or Y3, never Y0 or Y1.
    EXPECT_EQ(apply(dec, {X, H}), (std::vector<Signal>{L, L, X, X}));
    // A0 = 0, A1 unknown: Y0 or Y2.
    EXPECT_EQ(apply(dec, {L, X}), (std::vector<Signal>{X, L, X, L}));
    // Both unknown: any of them.
    EXPECT_EQ(apply(dec, {X, X}), (std::vector<Signal>{X, X, X, X}));
}

TEST(DecoderTest, BothSizesAreRegisteredAndSurviveSaveAndReopen)
{
    for (const char* type : {"DEC2", "DEC3"}) {
        Circuit c;
        std::unique_ptr<Component> part = ComponentFactory::standard().create(type);
        part->setLabel("U2");
        const ComponentId id = c.add(std::move(part));
        std::unique_ptr<Circuit> reopened = CircuitSerializer::fromJson(CircuitSerializer::toJson(c));
        ASSERT_NE(reopened->component(id), nullptr);
        EXPECT_EQ(reopened->component(id)->typeName(), type);
        EXPECT_EQ(reopened->component(id)->pinCount(), c.component(id)->pinCount());
        EXPECT_EQ(reopened->component(id)->label(), "U2");
    }
}
