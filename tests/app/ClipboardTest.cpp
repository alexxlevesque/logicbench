#include <gtest/gtest.h>

#include "../CircuitFixtures.h"

#include "CircuitClipboard.h"
#include "CircuitDocument.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QUndoStack>

#include <algorithm>

using namespace lbgui;

namespace {

QList<lb::ComponentId> idsOf(std::initializer_list<lb::ComponentId> ids)
{
    return QList<lb::ComponentId>(ids);
}

} // namespace

TEST(ClipboardTest, CopyPutsAFragmentOnTheClipboard)
{
    lb::Circuit c;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(c);
    ASSERT_TRUE(CircuitClipboard::copy(c, idsOf({h.a, h.b, h.xorGate})));
    EXPECT_TRUE(CircuitClipboard::hasFragment());

    std::unique_ptr<lb::Circuit> pasted = CircuitClipboard::paste();
    ASSERT_NE(pasted, nullptr);
    EXPECT_EQ(pasted->componentCount(), 3u);
    EXPECT_EQ(pasted->wires().size(), 2u);     // A->XOR and B->XOR; nothing to the LEDs
}

TEST(ClipboardTest, CopyingNothingLeavesTheClipboardAlone)
{
    lb::Circuit c;
    QGuiApplication::clipboard()->setText(QStringLiteral("unrelated"));
    EXPECT_FALSE(CircuitClipboard::copy(c, idsOf({42})));
    EXPECT_EQ(QGuiApplication::clipboard()->text(), QStringLiteral("unrelated"));
}

TEST(ClipboardTest, PlainTextIsNotAFragment)
{
    QGuiApplication::clipboard()->setText(QStringLiteral("AND gate"));
    EXPECT_FALSE(CircuitClipboard::hasFragment());
    EXPECT_EQ(CircuitClipboard::paste(), nullptr);
}

TEST(ClipboardTest, APasteIsOneUndoStepAndRedoReusesTheIds)
{
    CircuitDocument document;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(document.circuit());
    const lb::Circuit fragment = document.circuit().extract({h.xorGate, h.sumLed});
    const int before = document.undoStack()->count();

    const QList<lb::ComponentId> ids =
        document.insertFragment(fragment, QPointF(20, 20), QStringLiteral("paste"));
    ASSERT_EQ(ids.size(), 2);
    EXPECT_EQ(document.undoStack()->count(), before + 1);
    EXPECT_EQ(document.circuit().componentCount(), 8u);
    EXPECT_EQ(document.circuit().wires().size(), 7u);

    document.undoStack()->undo();
    EXPECT_EQ(document.circuit().componentCount(), 6u);
    EXPECT_EQ(document.circuit().wires().size(), 6u);

    document.undoStack()->redo();
    for (lb::ComponentId id : ids)
        EXPECT_TRUE(document.circuit().contains(id)) << id;
    EXPECT_EQ(document.circuit().wires().size(), 7u);
}

TEST(ClipboardTest, APastedHalfAdderWorks)
{
    CircuitDocument document;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(document.circuit());
    ASSERT_TRUE(CircuitClipboard::copy(document.circuit(),
                                       idsOf({h.a, h.b, h.xorGate, h.andGate, h.sumLed, h.carryLed})));
    std::unique_ptr<lb::Circuit> fragment = CircuitClipboard::paste();
    ASSERT_NE(fragment, nullptr);
    const QList<lb::ComponentId> ids = document.insertFragment(*fragment, QPointF(0, 200), QStringLiteral("paste"));
    ASSERT_EQ(ids.size(), 6);

    // Flip both pasted switches: the pasted carry LED lights, the original does not.
    for (lb::ComponentId id : ids) {
        if (document.circuit().component(id)->typeName() == "SWITCH")
            document.toggleSwitch(id);
    }
    const auto pastedCarry = std::find_if(ids.begin(), ids.end(), [&](lb::ComponentId id) {
        const lb::Component* c = document.circuit().component(id);
        return c->typeName() == "LED" && c->label() == "C";
    });
    ASSERT_NE(pastedCarry, ids.end());
    EXPECT_EQ(document.circuit().component(*pastedCarry)->pin(0).signal(), lb::Signal::High);
    EXPECT_NE(document.circuit().component(h.carryLed)->pin(0).signal(), lb::Signal::High);
}
