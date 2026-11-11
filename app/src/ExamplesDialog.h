#pragma once

#include <QDialog>

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

/// Examples and quick start (FR-3): the bundled circuits with a preview and
/// a description, and the six-step half-adder tutorial.
///
/// Shell for the SDD screen capture. The previews are real renders of the
/// bundled files; Open a copy is not wired in yet. When it is, the list comes
/// from ExampleCatalog and the ticks from QuickStartTracker.
class ExamplesDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { ExamplesTab = 0, QuickStartTab = 1 };

    /// examplesDirectory holds the bundled .lbc files.
    explicit ExamplesDialog(const QString& examplesDirectory, QWidget* parent = nullptr);

    void showTab(Tab tab);
    bool showAtStartup() const;

signals:
    /// Open a copy: the path of the bundled file to open as untitled.
    void openCopyRequested(const QString& path);

private slots:
    void showExample(int row);

private:
    QWidget* buildExamplesTab();
    QWidget* buildQuickStartTab();

    QString m_directory;
    QTabWidget* m_tabs;
    QListWidget* m_list;
    QLabel* m_preview;
    QLabel* m_title;
    QLabel* m_description;
    QCheckBox* m_atStartup;
};

} // namespace lbgui
