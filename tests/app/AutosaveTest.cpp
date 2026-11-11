#include <gtest/gtest.h>

#include "AppSettings.h"
#include "AutosaveManager.h"
#include "CircuitDocument.h"

#include "logicbench/IoComponents.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUndoStack>

using namespace lbgui;

namespace {

void addSwitch(CircuitDocument& document, const char* label)
{
    auto sw = std::make_unique<lb::Switch>();
    sw->setLabel(label);
    document.addComponent(std::move(sw), QPointF(0, 0));
}

} // namespace

TEST(AutosaveTest, NothingIsWrittenWhileThereAreNoUnsavedEdits)
{
    QTemporaryDir dir;
    CircuitDocument document;
    AutosaveManager autosave(&document, dir.path());
    autosave.writeRecovery();
    EXPECT_FALSE(autosave.hasRecovery());
    EXPECT_FALSE(autosave.timer()->isActive());
}

TEST(AutosaveTest, AnEditStartsTheTwoMinuteTimer)
{
    QTemporaryDir dir;
    CircuitDocument document;
    AutosaveManager autosave(&document, dir.path());
    addSwitch(document, "A");
    EXPECT_TRUE(autosave.timer()->isActive());
    EXPECT_EQ(autosave.timer()->interval(), 120000);
}

TEST(AutosaveTest, KillAndRelaunchOffersAndRestoresTheWork)
{
    QTemporaryDir dir;
    const QString original = QDir(dir.path()).filePath(QStringLiteral("mine.lbc"));
    {
        // The session that crashes: saved once, edited again, never closed.
        CircuitDocument document;
        AutosaveManager autosave(&document, dir.path());
        addSwitch(document, "A");
        ASSERT_TRUE(document.save(original, nullptr));
        addSwitch(document, "B");
        autosave.writeRecovery();                       // what the timer would have done
        ASSERT_TRUE(autosave.hasRecovery());
    }

    // The next launch.
    CircuitDocument relaunched;
    AutosaveManager autosave(&relaunched, dir.path());
    ASSERT_TRUE(autosave.hasRecovery());
    EXPECT_EQ(autosave.recoverySource(), original);
    EXPECT_TRUE(autosave.recoveryTime().isValid());

    QString error;
    ASSERT_TRUE(relaunched.restoreRecovery(autosave.recoveryPath(), autosave.recoverySource(), &error))
        << error.toStdString();
    EXPECT_EQ(relaunched.circuit().componentsOfType("SWITCH").size(), 2u);
    EXPECT_EQ(relaunched.filePath(), original);         // Save goes back where it belongs
    EXPECT_TRUE(relaunched.isModified());               // and Quit still asks
}

TEST(AutosaveTest, AnUntitledCircuitRestoresAsUntitled)
{
    QTemporaryDir dir;
    {
        CircuitDocument document;
        AutosaveManager autosave(&document, dir.path());
        addSwitch(document, "A");
        autosave.writeRecovery();
    }
    CircuitDocument relaunched;
    AutosaveManager autosave(&relaunched, dir.path());
    EXPECT_TRUE(autosave.recoverySource().isEmpty());
    ASSERT_TRUE(relaunched.restoreRecovery(autosave.recoveryPath(), QString(), nullptr));
    EXPECT_TRUE(relaunched.filePath().isEmpty());
}

TEST(AutosaveTest, SavingOrACleanQuitLeavesNoFile)
{
    QTemporaryDir dir;
    CircuitDocument document;
    AutosaveManager autosave(&document, dir.path());
    addSwitch(document, "A");
    autosave.writeRecovery();
    ASSERT_TRUE(autosave.hasRecovery());

    ASSERT_TRUE(document.save(QDir(dir.path()).filePath(QStringLiteral("saved.lbc")), nullptr));
    EXPECT_FALSE(autosave.hasRecovery());               // clean again: nothing to recover
    EXPECT_FALSE(autosave.timer()->isActive());

    addSwitch(document, "B");
    autosave.writeRecovery();
    ASSERT_TRUE(autosave.hasRecovery());
    autosave.discardRecovery();                         // what a clean quit does
    EXPECT_FALSE(autosave.hasRecovery());
    EXPECT_FALSE(QFile::exists(QDir(dir.path()).filePath(QStringLiteral("recovery.json"))));
}

TEST(AutosaveTest, UndoingBackToTheSavedStateDiscardsTheRecovery)
{
    QTemporaryDir dir;
    CircuitDocument document;
    AutosaveManager autosave(&document, dir.path());
    addSwitch(document, "A");
    autosave.writeRecovery();
    document.undoStack()->undo();                       // back to the clean empty circuit
    EXPECT_FALSE(autosave.hasRecovery());
}

TEST(AppSettingsTest, DefaultsAndRoundTrip)
{
    QTemporaryDir dir;
    const QString ini = QDir(dir.path()).filePath(QStringLiteral("settings.ini"));
    {
        AppSettings settings(ini);
        EXPECT_EQ(settings.tickIntervalMs(), 40);
        EXPECT_TRUE(settings.snapToGrid());
        EXPECT_TRUE(settings.showExamplesAtStartup());
        EXPECT_FALSE(settings.quickStartDone());
        EXPECT_TRUE(settings.blockLibraryDirectory().endsWith(QStringLiteral("LogicBench/blocks")));

        settings.setTickIntervalMs(5000);               // clamped to the legal range
        settings.setSnapToGrid(false);
        settings.setShowExamplesAtStartup(false);
        settings.setQuickStartDone(true);
        settings.setBlockLibraryDirectory(QStringLiteral("/tmp/blocks"));
        settings.sync();
    }
    AppSettings reread(ini);
    EXPECT_EQ(reread.tickIntervalMs(), 1000);
    EXPECT_FALSE(reread.snapToGrid());
    EXPECT_FALSE(reread.showExamplesAtStartup());
    EXPECT_TRUE(reread.quickStartDone());
    EXPECT_EQ(reread.blockLibraryDirectory(), QStringLiteral("/tmp/blocks"));
}
