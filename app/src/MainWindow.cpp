#include "MainWindow.h"

#include "CircuitDocument.h"
#include "BlockLibraryDialog.h"
#include "CircuitScene.h"
#include "ExamplesDialog.h"
#include "ExportImageDialog.h"
#include "ExpressionDialog.h"
#include "PaletteDock.h"
#include "PropertiesDock.h"
#include "SimplifyDialog.h"
#include "SpecCheckDialog.h"
#include "SubcircuitDialog.h"
#include "TruthTableDialog.h"
#include "WaveformDock.h"

#include "logicbench/CircuitSerializer.h"
#include "logicbench/SubCircuit.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>

namespace lbgui {

namespace {
constexpr const char* kFileFilter = "LogicBench circuits (*.lbc);;All files (*)";
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_document(new CircuitDocument(this))
    , m_scene(new CircuitScene(m_document, this))
    , m_view(new CircuitView(m_scene, this))
    , m_runTimer(new QTimer(this))
    , m_statusLabel(new QLabel(this))
    , m_simulationLabel(new QLabel(this))
{
    setCentralWidget(m_view);
    setWindowIcon(QIcon());
    resize(1180, 780);

    createActions();
    createMenus();
    createToolBar();
    createDocks();

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_simulationLabel);

    connect(m_scene, &CircuitScene::statusMessage, this, &MainWindow::updateStatus);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        m_properties->setSelection(m_scene->selectedComponentIds());
    });
    connect(m_document, &CircuitDocument::statusMessage, this, &MainWindow::updateStatus);
    connect(m_document->undoStack(), &QUndoStack::cleanChanged, this,
            &MainWindow::updateWindowTitle);
    connect(m_document, &CircuitDocument::signalsChanged, this, [this] {
        const int stuck = m_document->oscillating().size();
        m_simulationLabel->setText(
            stuck == 0 ? tr("settled") : tr("%n component(s) not settling", nullptr, stuck));
        m_simulationLabel->setStyleSheet(stuck == 0 ? QStringLiteral("color:#3C7A50;")
                                                    : QStringLiteral("color:#B3261E;"));
    });

    // The canvas should get the space; the docks are references, not the work.
    resizeDocks({m_palette}, {190}, Qt::Horizontal);
    resizeDocks({m_waveforms}, {170}, Qt::Vertical);

    // One timer tick is one simulated tick, which keeps the timing diagram's
    // x-axis and the clock's period in the same unit.
    m_runTimer->setInterval(m_tickIntervalMs);
    connect(m_runTimer, &QTimer::timeout, this, [this] { m_document->advance(1); });

    updateWindowTitle();
    updateStatus(tr("Drag a component from the palette onto the canvas to begin."));
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newCircuit);

    m_openAction = new QAction(tr("&Open…"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::open);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::save);

    m_saveAsAction = new QAction(tr("Save &As…"), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_undoAction = m_document->undoStack()->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction = m_document->undoStack()->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);

    m_deleteAction = new QAction(tr("&Delete"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered, m_scene, &CircuitScene::deleteSelection);

    m_selectAllAction = new QAction(tr("Select &All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, [this] {
        for (QGraphicsItem* item : m_scene->items())
            item->setSelected(true);
    });

    m_zoomInAction = new QAction(tr("Zoom &In"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, m_view, &CircuitView::zoomIn);

    m_zoomOutAction = new QAction(tr("Zoom &Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_view, &CircuitView::zoomOut);

    m_zoomResetAction = new QAction(tr("&Actual Size"), this);
    connect(m_zoomResetAction, &QAction::triggered, m_view, &CircuitView::resetZoom);

    m_fitAction = new QAction(tr("&Fit Circuit"), this);
    m_fitAction->setShortcut(Qt::CTRL | Qt::Key_0);
    connect(m_fitAction, &QAction::triggered, m_view, &CircuitView::frameCircuit);

    m_runAction = new QAction(tr("&Run"), this);
    m_runAction->setObjectName(QStringLiteral("runAction"));
    m_runAction->setCheckable(true);
    m_runAction->setShortcut(Qt::Key_F5);
    connect(m_runAction, &QAction::toggled, this, &MainWindow::toggleRun);

    m_stepAction = new QAction(tr("&Step"), this);
    m_stepAction->setObjectName(QStringLiteral("stepAction"));
    m_stepAction->setShortcut(Qt::Key_F6);
    connect(m_stepAction, &QAction::triggered, this, &MainWindow::stepOnce);

    m_resetAction = new QAction(tr("Rese&t simulation"), this);
    m_resetAction->setObjectName(QStringLiteral("resetAction"));
    connect(m_resetAction, &QAction::triggered, this, &MainWindow::resetSimulation);

    m_truthTableAction = new QAction(tr("&Truth table…"), this);
    m_truthTableAction->setObjectName(QStringLiteral("truthTableAction"));
    m_truthTableAction->setShortcut(Qt::CTRL | Qt::Key_T);
    connect(m_truthTableAction, &QAction::triggered, this, &MainWindow::showTruthTable);

    m_packageAction = new QAction(tr("&Package as block…"), this);
    connect(m_packageAction, &QAction::triggered, this, &MainWindow::packageAsBlock);

    m_placeBlockAction = new QAction(tr("Place &block from file…"), this);
    connect(m_placeBlockAction, &QAction::triggered, this, &MainWindow::placeBlockFromFile);

    m_preferencesAction = new QAction(tr("Pre&ferences…"), this);
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);

    m_aboutAction = new QAction(tr("&About LogicBench"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    m_exportImageAction = new QAction(tr("&Export image…"), this);
    connect(m_exportImageAction, &QAction::triggered, this, &MainWindow::showExportImage);

    m_simplifyAction = new QAction(tr("S&implify…"), this);
    connect(m_simplifyAction, &QAction::triggered, this, &MainWindow::showSimplify);

    m_specCheckAction = new QAction(tr("&Check against spec…"), this);
    connect(m_specCheckAction, &QAction::triggered, this, &MainWindow::showSpecCheck);

    m_buildExpressionAction = new QAction(tr("&Build from expression…"), this);
    connect(m_buildExpressionAction, &QAction::triggered, this,
            &MainWindow::showBuildFromExpression);

    m_blockLibraryAction = new QAction(tr("Block &library…"), this);
    connect(m_blockLibraryAction, &QAction::triggered, this, &MainWindow::showBlockLibrary);

    m_examplesAction = new QAction(tr("&Examples…"), this);
    connect(m_examplesAction, &QAction::triggered, this, &MainWindow::showExamples);

    m_quickStartAction = new QAction(tr("&Quick start…"), this);
    connect(m_quickStartAction, &QAction::triggered, this, &MainWindow::showQuickStart);
}

void MainWindow::createMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_newAction);
    file->addAction(m_openAction);
    file->addSeparator();
    file->addAction(m_saveAction);
    file->addAction(m_saveAsAction);
    file->addSeparator();
    file->addAction(m_exportImageAction);
    file->addSeparator();
    file->addAction(m_quitAction);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_undoAction);
    edit->addAction(m_redoAction);
    edit->addSeparator();
    edit->addAction(m_deleteAction);
    edit->addAction(m_selectAllAction);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_zoomInAction);
    view->addAction(m_zoomOutAction);
    view->addAction(m_zoomResetAction);
    view->addAction(m_fitAction);
    view->addSeparator();

    QMenu* tools = menuBar()->addMenu(tr("&Tools"));
    tools->addAction(m_runAction);
    tools->addAction(m_stepAction);
    tools->addAction(m_resetAction);
    tools->addSeparator();
    tools->addAction(m_truthTableAction);
    tools->addAction(m_simplifyAction);
    tools->addAction(m_specCheckAction);
    tools->addAction(m_buildExpressionAction);
    tools->addSeparator();
    tools->addAction(m_packageAction);
    tools->addAction(m_placeBlockAction);
    tools->addAction(m_blockLibraryAction);
    tools->addSeparator();
    tools->addAction(m_preferencesAction);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_examplesAction);
    help->addAction(m_quickStartAction);
    help->addSeparator();
    help->addAction(m_aboutAction);
}

void MainWindow::createToolBar()
{
    QToolBar* bar = addToolBar(tr("Main"));
    bar->setObjectName(QStringLiteral("mainToolBar"));
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    bar->addAction(m_undoAction);
    bar->addAction(m_redoAction);
    bar->addSeparator();
    bar->addAction(m_runAction);
    bar->addAction(m_stepAction);
    bar->addAction(m_resetAction);
    bar->addSeparator();
    bar->addAction(m_truthTableAction);
    bar->addAction(m_packageAction);
}

void MainWindow::createDocks()
{
    m_palette = new PaletteDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_palette);
    connect(m_palette, &PaletteDock::typeActivated, this, &MainWindow::placePayloadAtCentre);

    m_properties = new PropertiesDock(m_document, this);
    addDockWidget(Qt::RightDockWidgetArea, m_properties);

    m_waveforms = new WaveformDock(m_document, this);
    addDockWidget(Qt::BottomDockWidgetArea, m_waveforms);

    QMenu* view = menuBar()->findChild<QMenu*>();
    for (QAction* action : menuBar()->actions()) {
        if (action->text() == tr("&View") && action->menu()) {
            view = action->menu();
            break;
        }
    }
    if (view) {
        view->addAction(m_palette->toggleViewAction());
        view->addAction(m_properties->toggleViewAction());
        view->addAction(m_waveforms->toggleViewAction());
    }
}

// ------------------------------------------------------------- files -------

void MainWindow::newCircuit()
{
    if (!confirmDiscardChanges())
        return;
    m_runAction->setChecked(false);
    m_document->newCircuit();
    updateWindowTitle();
    updateStatus(tr("New circuit."));
}

void MainWindow::open()
{
    if (!confirmDiscardChanges())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open circuit"),
                                                      blocksDirectory(), tr(kFileFilter));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::openFile(const QString& path)
{
    QString error;
    if (!m_document->open(path, &error)) {
        QMessageBox::warning(this, tr("Cannot open circuit"),
                             tr("%1\n\n%2").arg(path, error));
        return;
    }
    m_runAction->setChecked(false);
    m_waveforms->clear();
    m_waveforms->refreshSignalList();
    m_view->frameCircuit();
    updateWindowTitle();
    updateStatus(tr("Opened %1").arg(path));
}

bool MainWindow::save()
{
    if (m_document->filePath().isEmpty())
        return saveAs();

    QString error;
    if (!m_document->save(m_document->filePath(), &error)) {
        QMessageBox::warning(this, tr("Cannot save"), error);
        return false;
    }
    updateWindowTitle();
    updateStatus(tr("Saved %1").arg(m_document->filePath()));
    return true;
}

bool MainWindow::saveAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save circuit"),
        QDir(blocksDirectory()).filePath(m_document->displayName()), tr(kFileFilter));
    if (path.isEmpty())
        return false;

    QString error;
    if (!m_document->save(path, &error)) {
        QMessageBox::warning(this, tr("Cannot save"), error);
        return false;
    }
    updateWindowTitle();
    updateStatus(tr("Saved %1").arg(path));
    return true;
}

bool MainWindow::confirmDiscardChanges()
{
    if (!m_document->isModified())
        return true;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Unsaved changes"),
        tr("%1 has unsaved changes.").arg(m_document->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (confirmDiscardChanges())
        event->accept();
    else
        event->ignore();
}

QString MainWindow::blocksDirectory() const
{
    if (!m_document->filePath().isEmpty())
        return QFileInfo(m_document->filePath()).absolutePath();
    return QDir::currentPath();
}

// -------------------------------------------------------- simulation -------

void MainWindow::toggleRun(bool running)
{
    if (running) {
        m_runTimer->start();
        m_runAction->setText(tr("&Pause"));
        updateStatus(tr("Running — clocks advance one tick every %1 ms.").arg(m_tickIntervalMs));
    } else {
        m_runTimer->stop();
        m_runAction->setText(tr("&Run"));
        updateStatus(tr("Paused."));
    }
}

void MainWindow::traceAllSignals()
{
    m_waveforms->selectAllSignals();
}

void MainWindow::simulateTicks(int ticks)
{
    for (int i = 0; i < ticks; ++i)
        m_document->advance(1);
}

void MainWindow::stepOnce()
{
    m_document->advance(1);
}

void MainWindow::resetSimulation()
{
    m_document->resimulate();
    m_waveforms->clear();
    updateStatus(tr("Simulation reset."));
}

// ------------------------------------------------------------- tools -------

void MainWindow::showTruthTable()
{
    TruthTableDialog dialog(m_document, this);
    dialog.exec();
}

void MainWindow::packageAsBlock()
{
    if (m_document->circuit().componentsOfType("SWITCH").empty()
        || m_document->circuit().componentsOfType("LED").empty()) {
        QMessageBox::information(
            this, tr("Nothing to package"),
            tr("A block needs at least one switch for an input pin and one LED "
               "for an output pin."));
        return;
    }

    SubcircuitDialog dialog(m_document, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    dialog.applyLabels();

    const QString suggestion = QDir(blocksDirectory()).filePath(dialog.blockName() + ".lbc");
    const QString path = QFileDialog::getSaveFileName(this, tr("Save block"), suggestion,
                                                      tr(kFileFilter));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_document->save(path, &error)) {
        QMessageBox::warning(this, tr("Cannot save block"), error);
        return;
    }

    // The block joins the palette for the rest of the session, so it can be
    // dragged into the next circuit without a trip through the file dialog.
    m_palette->addBlock(dialog.blockName(), path);
    updateWindowTitle();
    updateStatus(tr("Packaged as %1 — it is now in the palette under Blocks.")
                     .arg(dialog.blockName()));
}

void MainWindow::placeBlockFromFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Place block"),
                                                      blocksDirectory(), tr(kFileFilter));
    if (path.isEmpty())
        return;
    m_palette->addBlock(QFileInfo(path).completeBaseName(), path);
    placePayloadAtCentre(QStringLiteral("@block:") + path);
}

void MainWindow::placePayloadAtCentre(const QString& payload)
{
    try {
        std::unique_ptr<lb::Component> component = CircuitScene::createFromPayload(payload);
        const QPointF centre = m_view->mapToScene(m_view->viewport()->rect().center());
        m_document->addComponent(std::move(component), centre);
        updateStatus(tr("Placed %1.").arg(payload));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Cannot place component"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::showPreferences()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Preferences"));

    auto* interval = new QSpinBox(&dialog);
    interval->setRange(5, 1000);
    interval->setSuffix(tr(" ms"));
    interval->setValue(m_tickIntervalMs);
    interval->setToolTip(tr("Real time between simulated ticks while running."));

    auto* snap = new QCheckBox(tr("Snap components to the grid"), &dialog);
    snap->setChecked(m_scene->snapToGrid());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto* form = new QFormLayout(&dialog);
    form->addRow(tr("Tick interval"), interval);
    form->addRow(snap);
    form->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_tickIntervalMs = interval->value();
    m_runTimer->setInterval(m_tickIntervalMs);
    m_scene->setSnapToGrid(snap->isChecked());
}

// ----------------------------------------------- planned windows (shells) --

void MainWindow::showExportImage()
{
    ExportImageDialog dialog(!m_scene->selectedItems().isEmpty(), this);
    dialog.exec();
}

void MainWindow::showSimplify()
{
    SimplifyDialog dialog(m_document, this);
    connect(&dialog, &SimplifyDialog::buildRequested, this, [this](const QString& equations) {
        ExpressionDialog builder(m_document, this);
        builder.setEquations(equations);
        builder.exec();
    });
    dialog.exec();
}

void MainWindow::showSpecCheck()
{
    SpecCheckDialog dialog(m_document, this);
    dialog.exec();
}

void MainWindow::showBuildFromExpression()
{
    ExpressionDialog dialog(m_document, this);
    dialog.exec();
}

void MainWindow::showBlockLibrary()
{
    BlockLibraryDialog dialog(this);
    dialog.exec();
}

void MainWindow::showExamples()
{
    ExamplesDialog dialog(QStringLiteral(LOGICBENCH_EXAMPLES_DIR), this);
    dialog.exec();
}

void MainWindow::showQuickStart()
{
    ExamplesDialog dialog(QStringLiteral(LOGICBENCH_EXAMPLES_DIR), this);
    dialog.showTab(ExamplesDialog::QuickStartTab);
    dialog.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About LogicBench"),
        tr("<h3>LogicBench %1</h3>"
           "<p>A digital-logic simulator: drag gates onto the canvas, wire them "
           "together, flip the switches and watch the signals propagate.</p>"
           "<p><b>Getting started</b><br>"
           "Drag components from the palette. Drag from one pin to another to wire "
           "them. Click a switch to flip it. Tools &rarr; Truth table enumerates "
           "the inputs; Tools &rarr; Package as block turns the whole circuit into "
           "a reusable component.</p>"
           "<p>Signals are three-valued: <b>0</b>, <b>1</b> and <b>X</b> for "
           "undefined. A dashed red outline marks a feedback loop that cannot "
           "settle.</p>"
           "<p>Built for CISC/CMPE 320, Fall 2026.</p>")
            .arg(QApplication::applicationVersion()));
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(tr("%1%2 — LogicBench")
                       .arg(m_document->displayName(),
                            m_document->isModified() ? QStringLiteral("*") : QString()));
}

void MainWindow::updateStatus(const QString& message)
{
    m_statusLabel->setText(message);
}

} // namespace lbgui
