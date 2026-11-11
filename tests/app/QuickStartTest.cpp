#include <gtest/gtest.h>

#include "CircuitDocument.h"
#include "QuickStartTracker.h"

#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"

#include <QSignalSpy>

using namespace lbgui;

namespace {

lb::ComponentId place(CircuitDocument& d, std::unique_ptr<lb::Component> c, const char* label = "")
{
    c->setLabel(label);
    return d.addComponent(std::move(c), QPointF(0, 0));
}

} // namespace

TEST(QuickStartTest, EachStepTicksWhenItIsDone)
{
    CircuitDocument d;
    QuickStartTracker tracker(&d);
    EXPECT_EQ(tracker.doneCount(), 0);
    for (int i = 0; i < QuickStartTracker::kSteps; ++i)
        EXPECT_FALSE(QuickStartTracker::stepText(i).isEmpty());

    const lb::ComponentId a = place(d, std::make_unique<lb::Switch>(), "A");
    EXPECT_FALSE(tracker.isDone(0));
    const lb::ComponentId b = place(d, std::make_unique<lb::Switch>(), "B");
    EXPECT_TRUE(tracker.isDone(0));

    const lb::ComponentId x = place(d, std::make_unique<lb::XorGate>());
    const lb::ComponentId n = place(d, std::make_unique<lb::AndGate>());
    EXPECT_TRUE(tracker.isDone(1));

    d.connectPins({a, 0}, {x, 1});
    d.connectPins({b, 0}, {x, 2});
    d.connectPins({a, 0}, {n, 1});
    EXPECT_FALSE(tracker.isDone(2));
    d.connectPins({b, 0}, {n, 2});
    EXPECT_TRUE(tracker.isDone(2));

    const lb::ComponentId s = place(d, std::make_unique<lb::Led>(), "S");
    const lb::ComponentId c = place(d, std::make_unique<lb::Led>(), "C");
    EXPECT_TRUE(tracker.isDone(3));

    d.connectPins({x, 0}, {s, 0});
    d.connectPins({n, 0}, {c, 0});
    EXPECT_TRUE(tracker.isDone(4));
    EXPECT_FALSE(tracker.isDone(5));

    QSignalSpy spy(&tracker, &QuickStartTracker::progressed);
    tracker.markTruthTableOpened();
    EXPECT_TRUE(tracker.isDone(5));
    EXPECT_EQ(tracker.doneCount(), 6);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().first().toInt(), 5);
}

TEST(QuickStartTest, OpeningTheTruthTableTooEarlyDoesNotFinish)
{
    CircuitDocument d;
    QuickStartTracker tracker(&d);
    tracker.markTruthTableOpened();
    EXPECT_FALSE(tracker.isDone(5));
}

TEST(QuickStartTest, ATickStaysAfterAnUndoAndResetClearsIt)
{
    CircuitDocument d;
    QuickStartTracker tracker(&d);
    place(d, std::make_unique<lb::XorGate>());
    place(d, std::make_unique<lb::AndGate>());
    ASSERT_TRUE(tracker.isDone(1));
    d.undoStack()->undo();
    EXPECT_TRUE(tracker.isDone(1));
    tracker.reset();
    EXPECT_FALSE(tracker.isDone(1));
}
