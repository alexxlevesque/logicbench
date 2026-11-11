#include <gtest/gtest.h>

#include "CircuitDocument.h"

#include "logicbench/IoComponents.h"

#include <QUndoStack>

using namespace lbgui;

TEST(BuildFromEquationsTest, BuildsANewCircuitAsOneUndoStep)
{
    CircuitDocument document;
    QString error;
    ASSERT_TRUE(document.buildFromEquations(QStringLiteral("S = A ^ B\nC = A & B"), false, &error))
        << error.toStdString();
    EXPECT_EQ(document.circuit().componentsOfType("SWITCH").size(), 2u);
    EXPECT_EQ(document.circuit().componentsOfType("LED").size(), 2u);
    EXPECT_EQ(document.undoStack()->count(), 1);

    document.undoStack()->undo();
    EXPECT_EQ(document.circuit().componentCount(), 0u);
    document.undoStack()->redo();
    EXPECT_EQ(document.circuit().componentCount(), 6u);
}

TEST(BuildFromEquationsTest, TheBuiltCircuitIsLive)
{
    CircuitDocument document;
    ASSERT_TRUE(document.buildFromEquations(QStringLiteral("Y = A & B"), false));
    for (lb::Component* sw : document.circuit().componentsOfType("SWITCH"))
        document.toggleSwitch(sw->id());
    EXPECT_EQ(document.circuit().componentsOfType("LED").front()->pin(0).signal(), lb::Signal::High);
}

TEST(BuildFromEquationsTest, IntoTheOpenCircuitLandsBesideIt)
{
    CircuitDocument document;
    auto existing = std::make_unique<lb::Switch>();
    document.addComponent(std::move(existing), QPointF(300, 100));
    const std::size_t before = document.circuit().componentCount();

    ASSERT_TRUE(document.buildFromEquations(QStringLiteral("Y = !A"), true));
    EXPECT_EQ(document.circuit().componentCount(), before + 3);
    for (const auto& c : document.circuit().components()) {
        if (c->label() == "Y" || c->label() == "A")
            EXPECT_GT(c->x(), 300.0) << c->label();
    }
}

TEST(BuildFromEquationsTest, ASyntaxErrorChangesNothingAndNamesTheSpot)
{
    CircuitDocument document;
    document.addComponent(std::make_unique<lb::Led>(), QPointF(0, 0));
    QString error;
    EXPECT_FALSE(document.buildFromEquations(QStringLiteral("S = A ^\n"), false, &error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("Line 1, column 8"))) << error.toStdString();
    EXPECT_EQ(document.circuit().componentCount(), 1u);
}
