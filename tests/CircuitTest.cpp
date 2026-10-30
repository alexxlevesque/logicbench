#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/Circuit.h"
#include "logicbench/DFlipFlop.h"
#include "logicbench/Errors.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"

using namespace lb;

TEST(CircuitTest, AddAssignsUniqueIdsAndTakesOwnership)
{
    Circuit c;
    const ComponentId first = c.add(std::make_unique<AndGate>());
    const ComponentId second = c.add(std::make_unique<OrGate>());
    EXPECT_NE(first, kNoComponent);
    EXPECT_NE(first, second);
    EXPECT_EQ(c.componentCount(), 2u);
    EXPECT_EQ(c.component(first)->typeName(), "AND");
    EXPECT_EQ(c.component(second)->typeName(), "OR");
    EXPECT_EQ(c.component(9999), nullptr);
}

TEST(CircuitTest, AddHonoursAPreferredIdSoUndoCanRestoreWires)
{
    Circuit c;
    const ComponentId id = c.add(std::make_unique<AndGate>(), 42);
    EXPECT_EQ(id, 42u);
    // A second component cannot steal an id that is in use.
    EXPECT_NE(c.add(std::make_unique<OrGate>(), 42), 42u);
}

TEST(CircuitTest, ConnectRejectsBackwardsAndDoubleDrivenWires)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId sw2 = c.add(std::make_unique<Switch>());
    const ComponentId gate = c.add(std::make_unique<AndGate>());

    EXPECT_NO_THROW(c.connect({sw, 0}, {gate, 1}));

    // Output to output.
    EXPECT_THROW(c.connect({sw, 0}, {sw2, 0}), IllegalConnection);
    // Input to input.
    EXPECT_THROW(c.connect({gate, 1}, {gate, 2}), IllegalConnection);
    // A second driver on one input pin.
    EXPECT_THROW(c.connect({sw2, 0}, {gate, 1}), IllegalConnection);
    // Pin index out of range.
    EXPECT_THROW(c.connect({sw, 0}, {gate, 17}), IllegalConnection);
    // Component that is not in the circuit.
    EXPECT_THROW(c.connect({sw, 0}, {12345, 0}), IllegalConnection);

    EXPECT_EQ(c.wires().size(), 1u);
}

TEST(CircuitTest, OneOutputMayDriveManyInputs)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId g1 = c.add(std::make_unique<AndGate>());
    const ComponentId g2 = c.add(std::make_unique<OrGate>());

    c.connect({sw, 0}, {g1, 1});
    c.connect({sw, 0}, {g2, 1});
    EXPECT_EQ(c.sinksOf(PinRef{sw, 0}).size(), 2u);
}

TEST(CircuitTest, DeletingAComponentRemovesItsWires)
{
    // The classic dangling-wire bug: delete a wired gate and the wire list
    // still names it.
    Circuit c;
    lbtest::buildHalfAdder(c);
    const std::size_t before = c.wires().size();
    ASSERT_EQ(before, 6u);

    const ComponentId gate = c.componentsOfType("XOR").front()->id();
    ASSERT_TRUE(c.remove(gate));

    EXPECT_EQ(c.componentCount(), 5u);
    EXPECT_EQ(c.wires().size(), 3u);   // three wires touched the XOR
    for (const Wire& w : c.wires()) {
        EXPECT_NE(w.from.component, gate);
        EXPECT_NE(w.to.component, gate);
    }
}

TEST(CircuitTest, DeletingADriverLeavesTheDrivenInputFloating)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId gate = c.add(std::make_unique<AndGate>());
    dynamic_cast<Switch*>(c.component(sw))->setState(Signal::High);
    c.component(sw)->evaluate();
    c.connect({sw, 0}, {gate, 1});
    EXPECT_EQ(c.component(gate)->pin(1).signal(), Signal::High);

    c.remove(sw);
    EXPECT_EQ(c.component(gate)->pin(1).signal(), Signal::Undefined);
}

TEST(CircuitTest, TakeReturnsTheComponentSoUndoCanPutItBack)
{
    Circuit c;
    const ComponentId id = c.add(std::make_unique<AndGate>());
    std::unique_ptr<Component> owned = c.take(id);
    ASSERT_NE(owned, nullptr);
    EXPECT_EQ(c.componentCount(), 0u);

    const ComponentId restored = c.add(std::move(owned), id);
    EXPECT_EQ(restored, id);
    EXPECT_EQ(c.take(9999), nullptr);
}

TEST(CircuitTest, DisconnectFloatsTheInputAgain)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId gate = c.add(std::make_unique<AndGate>());
    dynamic_cast<Switch*>(c.component(sw))->setState(Signal::High);
    c.component(sw)->evaluate();
    const WireId wire = c.connect({sw, 0}, {gate, 1});

    EXPECT_TRUE(c.disconnect(wire));
    EXPECT_EQ(c.component(gate)->pin(1).signal(), Signal::Undefined);
    EXPECT_FALSE(c.disconnect(wire));   // already gone
}

TEST(CircuitTest, ShrinkingAGateRemovesWiresIntoTheLostPins)
{
    Circuit c;
    const ComponentId sw = c.add(std::make_unique<Switch>());
    const ComponentId gate = c.add(std::make_unique<AndGate>());
    c.setInputCount(gate, 3);
    c.connect({sw, 0}, {gate, 3});
    ASSERT_EQ(c.wires().size(), 1u);

    const std::vector<Wire> removed = c.setInputCount(gate, 2);
    EXPECT_EQ(removed.size(), 1u);
    EXPECT_TRUE(c.wires().empty());
}

TEST(CircuitTest, CopyIsADeepCopy)
{
    // Big Three territory: the copy must own its own components, or editing
    // one circuit changes the other and deleting one destroys both.
    Circuit original;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(original);

    Circuit copy = original;
    EXPECT_EQ(copy.componentCount(), original.componentCount());
    EXPECT_EQ(copy.wires().size(), original.wires().size());
    EXPECT_NE(copy.component(h.a), original.component(h.a));   // different objects

    copy.component(h.a)->setLabel("changed");
    EXPECT_EQ(original.component(h.a)->label(), "A");

    copy.remove(h.xorGate);
    EXPECT_EQ(original.componentCount(), 6u);
    EXPECT_NE(original.component(h.xorGate), nullptr);
}

TEST(CircuitTest, CopyAssignmentIsDeepAndSelfAssignmentIsSafe)
{
    Circuit original;
    lbtest::buildHalfAdder(original);

    Circuit other;
    other.add(std::make_unique<NotGate>());
    other = original;
    EXPECT_EQ(other.componentCount(), 6u);

    Circuit& alias = other;
    other = alias;                      // self-assignment must not wipe it out
    EXPECT_EQ(other.componentCount(), 6u);
    EXPECT_EQ(other.wires().size(), 6u);
}

TEST(CircuitTest, SequentialDetection)
{
    Circuit combinational;
    lbtest::buildHalfAdder(combinational);
    EXPECT_FALSE(combinational.hasSequentialParts());

    combinational.add(std::make_unique<DFlipFlop>());
    EXPECT_TRUE(combinational.hasSequentialParts());
}
