#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/DFlipFlop.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/SubCircuit.h"

#include <memory>

using namespace lb;

namespace {

/// A half adder packaged as a block: inputs A, B; outputs C, S.
std::unique_ptr<SubCircuit> makeHalfAdderBlock()
{
    auto inner = std::make_unique<Circuit>();
    inner->setName("half adder");
    lbtest::buildHalfAdder(*inner);
    return std::make_unique<SubCircuit>("half adder", std::move(inner));
}

void driveBlock(SubCircuit& block, Signal a, Signal b)
{
    block.pin(static_cast<std::size_t>(block.pinIndex("A"))).setSignal(a);
    block.pin(static_cast<std::size_t>(block.pinIndex("B"))).setSignal(b);
    block.evaluate();
}

Signal blockPin(const SubCircuit& block, const char* name)
{
    return block.pin(static_cast<std::size_t>(block.pinIndex(name))).signal();
}

} // namespace

TEST(SubCircuitTest, PinsComeFromTheInnerSwitchesAndLeds)
{
    std::unique_ptr<SubCircuit> block = makeHalfAdderBlock();
    EXPECT_EQ(block->typeName(), "SUBCIRCUIT");
    EXPECT_EQ(block->blockName(), "half adder");
    EXPECT_EQ(block->pinCount(), 4u);

    ASSERT_GE(block->pinIndex("A"), 0);
    ASSERT_GE(block->pinIndex("B"), 0);
    ASSERT_GE(block->pinIndex("S"), 0);
    ASSERT_GE(block->pinIndex("C"), 0);
    EXPECT_TRUE(block->pin(static_cast<std::size_t>(block->pinIndex("A"))).isInput());
    EXPECT_TRUE(block->pin(static_cast<std::size_t>(block->pinIndex("S"))).isOutput());
}

TEST(SubCircuitTest, BlockComputesWhatTheInnerCircuitComputes)
{
    std::unique_ptr<SubCircuit> block = makeHalfAdderBlock();

    driveBlock(*block, Signal::Low, Signal::Low);
    EXPECT_EQ(blockPin(*block, "S"), Signal::Low);
    EXPECT_EQ(blockPin(*block, "C"), Signal::Low);

    driveBlock(*block, Signal::High, Signal::Low);
    EXPECT_EQ(blockPin(*block, "S"), Signal::High);
    EXPECT_EQ(blockPin(*block, "C"), Signal::Low);

    driveBlock(*block, Signal::High, Signal::High);
    EXPECT_EQ(blockPin(*block, "S"), Signal::Low);
    EXPECT_EQ(blockPin(*block, "C"), Signal::High);
}

TEST(SubCircuitTest, CloningABlockDeepCopiesItsContents)
{
    // The demo case: place the same block twice, edit one, and the other must
    // not change with it.
    std::unique_ptr<SubCircuit> original = makeHalfAdderBlock();
    std::unique_ptr<Component> copy = original->clone();
    auto* clone = dynamic_cast<SubCircuit*>(copy.get());
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->inner().componentCount(), original->inner().componentCount());
    EXPECT_NE(&clone->inner(), &original->inner());
    for (const std::unique_ptr<Component>& c : original->inner().components())
        EXPECT_NE(clone->inner().component(c->id()), c.get());

    const ComponentId victim = original->inner().componentsOfType("XOR").front()->id();
    original->inner().remove(victim);
    EXPECT_NE(clone->inner().component(victim), nullptr)
        << "deleting inside one block must not reach into the copy";
}

TEST(SubCircuitTest, TwoInstancesHoldIndependentState)
{
    std::unique_ptr<SubCircuit> first = makeHalfAdderBlock();
    std::unique_ptr<Component> secondOwner = first->clone();
    auto* second = dynamic_cast<SubCircuit*>(secondOwner.get());

    driveBlock(*first, Signal::High, Signal::High);
    driveBlock(*second, Signal::Low, Signal::Low);

    EXPECT_EQ(blockPin(*first, "C"), Signal::High);
    EXPECT_EQ(blockPin(*second, "C"), Signal::Low);
}

TEST(SubCircuitTest, BlocksNestAndAFullAdderIsTwoHalfAddersPlusAnOr)
{
    // full adder: A, B, Cin -> S, Cout, built from two packaged half adders.
    Circuit outer;
    const ComponentId a = outer.add([] {
        auto s = std::make_unique<Switch>();
        s->setLabel("A");
        return s;
    }());
    const ComponentId b = outer.add([] {
        auto s = std::make_unique<Switch>();
        s->setLabel("B");
        return s;
    }());
    const ComponentId cin = outer.add([] {
        auto s = std::make_unique<Switch>();
        s->setLabel("Cin");
        return s;
    }());

    const ComponentId ha1 = outer.add(makeHalfAdderBlock());
    const ComponentId ha2 = outer.add(makeHalfAdderBlock());
    const ComponentId orGate = outer.add(std::make_unique<OrGate>());

    const ComponentId sumLed = outer.add([] {
        auto l = std::make_unique<Led>();
        l->setLabel("S");
        return l;
    }());
    const ComponentId coutLed = outer.add([] {
        auto l = std::make_unique<Led>();
        l->setLabel("Cout");
        return l;
    }());

    const auto pinOf = [&outer](ComponentId id, const char* name) {
        return PinRef{id, outer.component(id)->pinIndex(name)};
    };

    outer.connect({a, 0}, pinOf(ha1, "A"));
    outer.connect({b, 0}, pinOf(ha1, "B"));
    outer.connect(pinOf(ha1, "S"), pinOf(ha2, "A"));
    outer.connect({cin, 0}, pinOf(ha2, "B"));
    outer.connect(pinOf(ha2, "S"), {sumLed, 0});
    outer.connect(pinOf(ha1, "C"), {orGate, 1});
    outer.connect(pinOf(ha2, "C"), {orGate, 2});
    outer.connect({orGate, 0}, {coutLed, 0});

    Simulator sim(outer);
    sim.reset();

    for (int value = 0; value < 8; ++value) {
        const bool bitA = value & 4;
        const bool bitB = value & 2;
        const bool bitC = value & 1;
        lbtest::setSwitch(outer, sim, a, bitA);
        lbtest::setSwitch(outer, sim, b, bitB);
        lbtest::setSwitch(outer, sim, cin, bitC);

        const int total = static_cast<int>(bitA) + static_cast<int>(bitB) + static_cast<int>(bitC);
        EXPECT_EQ(lbtest::ledValue(outer, sumLed), signalFromBool(total % 2 == 1))
            << "A=" << bitA << " B=" << bitB << " Cin=" << bitC;
        EXPECT_EQ(lbtest::ledValue(outer, coutLed), signalFromBool(total >= 2))
            << "A=" << bitA << " B=" << bitB << " Cin=" << bitC;
    }
}

TEST(SubCircuitTest, ABlockContainingAFlipFlopReportsItselfSequential)
{
    auto inner = std::make_unique<Circuit>();
    inner->add(std::make_unique<Switch>());
    inner->add(std::make_unique<DFlipFlop>());
    inner->add(std::make_unique<Led>());
    SubCircuit block("register", std::move(inner));
    EXPECT_TRUE(block.isSequential());

    EXPECT_FALSE(makeHalfAdderBlock()->isSequential());
}

TEST(SubCircuitTest, AnEmptyBlockHasNoPinsAndDoesNotCrash)
{
    SubCircuit block;
    EXPECT_EQ(block.pinCount(), 0u);
    EXPECT_FALSE(block.evaluate());
    EXPECT_EQ(block.innerComponentForPin(0), kNoComponent);
}

TEST(SubCircuitTest, UnlabelledPortsStillGetStablePinNames)
{
    auto inner = std::make_unique<Circuit>();
    const ComponentId sw = inner->add(std::make_unique<Switch>());
    inner->add(std::make_unique<Led>());
    SubCircuit block("unnamed", std::move(inner));

    EXPECT_EQ(block.pinCount(), 2u);
    EXPECT_EQ(block.pin(0).name(), "IN" + std::to_string(sw));
}
