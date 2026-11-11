// One test per window wired in sprint 1: each drives the real dialog against a
// real document, the way the menus open it.

#include <gtest/gtest.h>

#include "CircuitDocument.h"
#include "ExamplesDialog.h"
#include "ExpressionDialog.h"
#include "QuickStartTracker.h"
#include "SimplifyDialog.h"
#include "SpecCheckDialog.h"

#include "logicbench/IoComponents.h"
#include "logicbench/TruthTable.h"

#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace lbgui;

namespace {

QPushButton* button(QWidget& dialog, const QString& text)
{
    for (QPushButton* b : dialog.findChildren<QPushButton*>())
        if (b->text() == text)
            return b;
    return nullptr;
}

/// The truth table of every LED over every switch, as CSV.
std::string tableOf(lb::Circuit& c)
{
    return lb::TruthTableGenerator::generate(c, lb::TruthTableGenerator::allSwitches(c),
                                             lb::TruthTableGenerator::allLeds(c)).table.toCsv();
}

const QString kFullAdder = QStringLiteral("S = A ^ B ^ Cin\nCout = (A & B) | (Cin & (A ^ B))");

} // namespace

TEST(DialogsTest, BuildFromExpressionChecksAsYouTypeAndBuilds)
{
    CircuitDocument document;
    ExpressionDialog dialog(&document);

    dialog.setEquations(QStringLiteral("S = A ^"));
    EXPECT_FALSE(dialog.canBuild());
    EXPECT_EQ(dialog.errorLine(), 1);

    dialog.setEquations(kFullAdder);
    ASSERT_TRUE(dialog.canBuild());
    QTest::mouseClick(button(dialog, QStringLiteral("Build")), Qt::LeftButton);
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(document.circuit().componentsOfType("SWITCH").size(), 3u);
    EXPECT_EQ(document.circuit().componentsOfType("LED").size(), 2u);
}

TEST(DialogsTest, SimplifyShowsEachOutputAndBuildsTheSameFunction)
{
    CircuitDocument document;
    ASSERT_TRUE(document.buildFromEquations(kFullAdder, false));
    SimplifyDialog dialog(&document);
    ASSERT_TRUE(dialog.result().ok());
    ASSERT_EQ(dialog.result().outputs.size(), 2u);
    EXPECT_TRUE(dialog.expressionText().startsWith(QStringLiteral("S = ")));

    // Build minimized circuit hands over equations that rebuild the same truth table.
    QSignalSpy spy(&dialog, &SimplifyDialog::buildRequested);
    QTest::mouseClick(button(dialog, QStringLiteral("Build minimized circuit")), Qt::LeftButton);
    ASSERT_EQ(spy.count(), 1);
    CircuitDocument rebuilt;
    ASSERT_TRUE(rebuilt.buildFromEquations(spy.first().first().toString(), false));
    EXPECT_EQ(tableOf(rebuilt.circuit()), tableOf(document.circuit()));
}

TEST(DialogsTest, SimplifyRefusesWithTheReason)
{
    CircuitDocument document;               // nothing to simplify
    SimplifyDialog dialog(&document);
    EXPECT_FALSE(dialog.result().ok());
    EXPECT_FALSE(button(dialog, QStringLiteral("Build minimized circuit"))->isEnabled());
}

TEST(DialogsTest, CheckAgainstSpecListsTheDifferingRows)
{
    QTemporaryDir dir;
    const QString spec = QDir(dir.path()).filePath(QStringLiteral("and_spec.csv"));
    QFile file(spec);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("A,B,Y\n0,0,0\n0,1,0\n1,0,0\n1,1,1\n");
    file.close();

    CircuitDocument document;
    ASSERT_TRUE(document.buildFromEquations(QStringLiteral("Y = A | B"), false));   // the wrong gate
    SpecCheckDialog dialog(&document);
    dialog.setSpecPath(spec);
    EXPECT_EQ(dialog.lastResult().status, lb::SpecCheckStatus::Fail);
    EXPECT_EQ(dialog.lastResult().rowsDiffering, 2u);
    EXPECT_EQ(dialog.rowCount(), 2);         // only the differing rows, by default

    // Fix the circuit and re-check without picking the file again.
    ASSERT_TRUE(document.buildFromEquations(QStringLiteral("Y = A & B"), false));
    QTest::mouseClick(button(dialog, QStringLiteral("Re-check")), Qt::LeftButton);
    EXPECT_TRUE(dialog.lastResult().passed());
}

TEST(DialogsTest, ExamplesOpensACopyAndTheQuickStartTicks)
{
    CircuitDocument document;
    QuickStartTracker tracker(&document);
    ExamplesDialog dialog(QStringLiteral(LOGICBENCH_EXAMPLES_DIR), &tracker);
    EXPECT_EQ(dialog.exampleCount(), 6);

    QSignalSpy spy(&dialog, &ExamplesDialog::openCopyRequested);
    dialog.selectExample(1);
    button(dialog, QStringLiteral("Open a copy"))->click();     // on a tab page: click directly
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.first().first().toString().endsWith(QStringLiteral("full_adder.lbc")));

    EXPECT_EQ(dialog.stepsTicked(), 0);
    auto a = std::make_unique<lb::Switch>();
    a->setLabel("A");
    auto b = std::make_unique<lb::Switch>();
    b->setLabel("B");
    document.addComponent(std::move(a), QPointF(0, 0));
    document.addComponent(std::move(b), QPointF(0, 60));
    EXPECT_EQ(dialog.stepsTicked(), 1);
}
