#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/Circuit.h"
#include "logicbench/CircuitSerializer.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/Note.h"

#include <algorithm>
#include <memory>

using namespace lb;

namespace {

bool hasWireBetween(const Circuit& c, ComponentId from, ComponentId to)
{
    return std::any_of(c.wires().begin(), c.wires().end(), [&](const Wire& w) {
        return w.from.component == from && w.to.component == to;
    });
}

} // namespace

TEST(FragmentTest, ExtractKeepsInnerWiresAndDropsOuterOnes)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);

    // The XOR gate and its LED: the wire between them is inner, the wires from
    // the switches into the XOR are outer.
    const Circuit fragment = c.extract({h.xorGate, h.sumLed});
    EXPECT_EQ(fragment.componentCount(), 2u);
    ASSERT_EQ(fragment.wires().size(), 1u);
    EXPECT_TRUE(hasWireBetween(fragment, h.xorGate, h.sumLed));
    // The source circuit is untouched.
    EXPECT_EQ(c.componentCount(), 6u);
    EXPECT_EQ(c.wires().size(), 6u);
}

TEST(FragmentTest, ExtractIgnoresUnknownIds)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    const Circuit fragment = c.extract({h.a, 9999});
    EXPECT_EQ(fragment.componentCount(), 1u);
}

TEST(FragmentTest, PasteGetsNewIdsAndKeepsItsInnerWires)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    const Circuit fragment = c.extract({h.a, h.b, h.xorGate, h.sumLed});

    const FragmentIds ids = c.insert(fragment, 40.0, 20.0);
    ASSERT_EQ(ids.components.size(), 4u);
    ASSERT_EQ(ids.wires.size(), 3u);            // A->XOR, B->XOR, XOR->S
    for (ComponentId id : ids.components) {
        EXPECT_NE(id, h.a);
        EXPECT_NE(id, h.b);
        EXPECT_NE(id, h.xorGate);
        EXPECT_NE(id, h.sumLed);
        EXPECT_TRUE(c.contains(id));
    }
    EXPECT_EQ(c.componentCount(), 10u);
    EXPECT_EQ(c.wires().size(), 9u);

    // The pasted copy is wired to itself, not to the original.
    for (const Wire& w : c.wires()) {
        const bool fromCopy = std::count(ids.components.begin(), ids.components.end(), w.from.component) > 0;
        const bool toCopy = std::count(ids.components.begin(), ids.components.end(), w.to.component) > 0;
        EXPECT_EQ(fromCopy, toCopy);
    }
}

TEST(FragmentTest, PasteMovesThePartsByTheOffset)
{
    Circuit c;
    auto sw = std::make_unique<Switch>();
    sw->setPosition(100.0, 50.0);
    const ComponentId id = c.add(std::move(sw));

    const FragmentIds ids = c.insert(c.extract({id}), 30.0, -10.0);
    const Component* copy = c.component(ids.components.front());
    EXPECT_DOUBLE_EQ(copy->x(), 130.0);
    EXPECT_DOUBLE_EQ(copy->y(), 40.0);
}

TEST(FragmentTest, PreferredIdsAreReusedSoRedoRestoresTheSameIds)
{
    Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    const Circuit fragment = c.extract({h.xorGate, h.sumLed});

    const FragmentIds first = c.insert(fragment, 10.0, 10.0);
    // Undo: remove exactly what was inserted.
    for (ComponentId id : first.components)
        c.remove(id);
    EXPECT_EQ(c.componentCount(), 6u);

    // Redo with the same ids preferred.
    const FragmentIds second = c.insert(fragment, 10.0, 10.0, first);
    EXPECT_EQ(second.components, first.components);
    EXPECT_EQ(second.wires, first.wires);
}

TEST(FragmentTest, PastingIntoAnEmptyCircuitWorks)
{
    Circuit source;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(source);
    const Circuit fragment = source.extract({h.a, h.b, h.xorGate, h.andGate, h.sumLed, h.carryLed});

    Circuit target;
    const FragmentIds ids = target.insert(fragment);
    EXPECT_EQ(target.componentCount(), 6u);
    EXPECT_EQ(target.wires().size(), 6u);
    EXPECT_EQ(ids.components.size(), 6u);
}

TEST(FragmentTest, NotesHaveNoPinsAndSaveTheirText)
{
    Circuit c;
    auto note = std::make_unique<Note>();
    note->setText("Carry ripples left to right");
    note->setPosition(20.0, 200.0);
    const ComponentId id = c.add(std::move(note));
    EXPECT_EQ(c.component(id)->pinCount(), 0u);

    std::unique_ptr<Circuit> reopened = CircuitSerializer::fromJson(CircuitSerializer::toJson(c));
    auto* back = dynamic_cast<Note*>(reopened->component(id));
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(back->text(), "Carry ripples left to right");
    EXPECT_DOUBLE_EQ(back->y(), 200.0);
}

TEST(FragmentTest, NotesCopyAndPasteLikeAnyComponent)
{
    Circuit c;
    std::unique_ptr<Component> note = ComponentFactory::standard().create("NOTE");
    note->setProperty("text", "check this");
    const ComponentId id = c.add(std::move(note));

    const FragmentIds ids = c.insert(c.extract({id}), 0.0, 30.0);
    auto* copy = dynamic_cast<Note*>(c.component(ids.components.front()));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->text(), "check this");
}
