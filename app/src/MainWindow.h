#pragma once

#include "logicbench/Pin.h"

#include <QList>
#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QTimer;

namespace lbgui {

class CircuitDocument;
class CircuitScene;
class CircuitView;
class PaletteDock;
class PropertiesDock;
class WaveformDock;

/// Menus, toolbar, docks and the run timer.
///
/// The window owns the document and wires the pieces together; it contains no
/// circuit logic of its own, which is what keeps the file short enough to read.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Opens a circuit file, reporting failures in a dialog.
    void openFile(const QString& path);

    /// Advances the simulation by n ticks without starting the run timer.
    /// Used by the --screenshot path and by anything scripting the window.
    void simulateTicks(int ticks);

    /// Plots every available signal on the timing diagram.
    void traceAllSignals();

    /// The open circuit. Exposed so a screenshot run can build the same
    /// dialogs the menus build.
    CircuitDocument* document() const { return m_document; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newCircuit();
    void open();
    void cutSelection();
    void copySelection();
    void paste();
    void duplicateSelection();
    void addNote();
    bool save();
    bool saveAs();
    void toggleRun(bool running);
    void stepOnce();
    void resetSimulation();
    void showTruthTable();
    void packageAsBlock();
    void placeBlockFromFile();
    void showPreferences();
    void showExportImage();
    void showSimplify();
    void showSpecCheck();
    void showBuildFromExpression();
    void showBlockLibrary();
    void showExamples();
    void showQuickStart();
    void showAbout();
    void updateWindowTitle();
    void updateStatus(const QString& message);

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createDocks();
    void placePayloadAtCentre(const QString& payload);
    bool confirmDiscardChanges();
    void selectComponents(const QList<lb::ComponentId>& ids);
    QString blocksDirectory() const;

    CircuitDocument* m_document;
    CircuitScene* m_scene;
    CircuitView* m_view;
    PaletteDock* m_palette;
    PropertiesDock* m_properties;
    WaveformDock* m_waveforms;
    QTimer* m_runTimer;
    QLabel* m_statusLabel;
    QLabel* m_simulationLabel;

    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_quitAction;
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_duplicateAction;
    QAction* m_addNoteAction;
    QAction* m_deleteAction;
    QAction* m_selectAllAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_zoomResetAction;
    QAction* m_fitAction;
    QAction* m_runAction;
    QAction* m_stepAction;
    QAction* m_resetAction;
    QAction* m_truthTableAction;
    QAction* m_packageAction;
    QAction* m_placeBlockAction;
    QAction* m_preferencesAction;
    QAction* m_aboutAction;
    // Planned windows (FR-3 to FR-8), opened as shells until wired in.
    QAction* m_exportImageAction;
    QAction* m_simplifyAction;
    QAction* m_specCheckAction;
    QAction* m_buildExpressionAction;
    QAction* m_blockLibraryAction;
    QAction* m_examplesAction;
    QAction* m_quickStartAction;

    int m_tickIntervalMs = 40;
    int m_pasteCount = 0;   ///< pastes since the last copy, so each lands a step further on
};

} // namespace lbgui
