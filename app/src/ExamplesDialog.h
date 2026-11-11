#pragma once

#include "ExampleCatalog.h"

#include <QDialog>
#include <QVector>

// Set by app/CMakeLists.txt; targets that compile the app sources without it
// (the GUI tests) fall back to a path relative to the working directory.
#ifndef LOGICBENCH_EXAMPLES_DIR
#define LOGICBENCH_EXAMPLES_DIR "examples"
#endif

class QCheckBox;
class QLabel;
class QListWidget;
class QTabWidget;

namespace lbgui {

class QuickStartTracker;

/// Examples and quick start (FR-3): the bundled circuits with a preview and
/// a description, and the six-step half-adder tutorial.
///
/// The list and descriptions come from ExampleCatalog and the previews are
/// renders of the files; Open a copy opens one as an untitled circuit. The
/// Quick start tab's ticks come from QuickStartTracker and update while the
/// window stays open beside the canvas.
class ExamplesDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { ExamplesTab = 0, QuickStartTab = 1 };

    /// examplesDirectory holds the bundled .lbc files.
    /// examplesDirectory holds the bundled .lbc files and catalog.json;
    /// tracker, if given, ticks the quick-start steps.
    ExamplesDialog(const QString& examplesDirectory, QuickStartTracker* tracker,
                   QWidget* parent = nullptr);

    void showTab(Tab tab);
    bool showAtStartup() const;
    void setShowAtStartup(bool on);
    void selectExample(int row);

    int exampleCount() const;
    int stepsTicked() const;

signals:
    /// Open a copy: the path of the bundled file to open as untitled.
    void openCopyRequested(const QString& path);

private slots:
    void showExample(int row);
    void refreshSteps();

private:
    QWidget* buildExamplesTab();
    QWidget* buildQuickStartTab();

    QString m_directory;
    ExampleCatalog m_catalog;
    QuickStartTracker* m_tracker;
    QVector<QCheckBox*> m_steps;
    QLabel* m_progress = nullptr;
    QTabWidget* m_tabs;
    QListWidget* m_list;
    QLabel* m_preview;
    QLabel* m_title;
    QLabel* m_description;
    QCheckBox* m_atStartup;
};

} // namespace lbgui
