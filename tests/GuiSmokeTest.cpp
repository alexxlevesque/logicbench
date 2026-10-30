// Drives the canvas with real mouse events through the view, so the parts that
// unit tests cannot reach — wiring by dragging, poking a switch, moving a
// selection, undoing a delete — are checked rather than assumed.
//
// Runs headless under "-platform offscreen", so it belongs in CI as much as
// the engine suite does.

#include "CircuitDocument.h"
#include "CircuitScene.h"
#include "ComponentItem.h"
#include "MainWindow.h"
#include "TruthTableDialog.h"
#include "WaveformDock.h"
#include "WireItem.h"

#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"

#include <QAction>
#include <QDir>
#include <QGraphicsView>
#include <QLabel>
#include <QPointer>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>
#include <QUndoStack>
#include <QTimer>
#include <QVBoxLayout>

using namespace lbgui;

// QCOMPARE cannot format lb::Signal directly: its unqualified toString call
// finds lb::toString through argument-dependent lookup, and that returns a
// std::string where QTest wants a char*. Comparing the display character
// sidesteps it and reads better in a failure message anyway.
namespace {

char level(const lb::Component* component, int pin)
{
    return component ? lb::toChar(component->pin(static_cast<std::size_t>(pin)).signal()) : '?';
}

/// Setting LOGICBENCH_DEMO_MS slows the suite down and pins the window to a
/// fixed spot on screen, so the same tests can be screen-recorded. Unset — the
/// way CI runs them — every wait is zero and nothing moves.
int demoDelayMs()
{
    static const int ms = qEnvironmentVariableIntValue("LOGICBENCH_DEMO_MS");
    return ms;
}

bool demoMode()
{
    return demoDelayMs() > 0;
}

/// Setting LOGICBENCH_DEMO_FRAMES to a directory makes the suite save a frame
/// of the window under test every few milliseconds, which is how the
/// documentation's animations are produced. Grabbing the widget rather than
/// the screen means the frames contain the application and nothing else, and
/// it works offscreen.
QString framesDirectory()
{
    return qEnvironmentVariable("LOGICBENCH_DEMO_FRAMES");
}

bool capturingFrames()
{
    return !framesDirectory().isEmpty();
}

constexpr int kFrameIntervalMs = 80;

constexpr int kDemoWindowX = 40;
constexpr int kDemoWindowY = 90;
constexpr int kDemoWindowW = 1000;
constexpr int kDemoWindowH = 620;

} // namespace

class GuiSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void wiringByDragCreatesAConnection();
    void wiringBackwardsIsAcceptedAndNormalised();
    void anIllegalConnectionIsRefusedWithAMessage();
    void clickingASwitchTogglesItAndTheLedFollows();
    void draggingAComponentIsOneUndoStep();
    void deletingAComponentAndUndoingRestoresItsWires();

    void runningTheClockAdvancesTheTimingDiagram();
    void theTruthTableDialogFillsFromTheCircuit();

private:
    QPoint viewPointOfPin(lb::ComponentId id, int pin) const;
    void dragBetweenPins(lb::ComponentId fromId, int fromPin,
                         lb::ComponentId toId, int toPin);
    void narrate(const QString& text);
    void settleFrames(int ms = 0);
    void placeForDemo(QWidget* widget);
    void startCapture(QWidget* target);
    void stopCapture();
    void captureFrame();

    QWidget* m_window = nullptr;
    QLabel* m_narration = nullptr;
    // A guarded pointer, not a raw one: the whole-window tests capture a
    // widget that lives on their own stack, and the capture timer keeps
    // firing while it is being torn down.
    QPointer<QWidget> m_captureTarget;
    QTimer* m_captureTimer = nullptr;
    QString m_captureDir;
    int m_frameNumber = 0;
    CircuitDocument* m_document = nullptr;
    CircuitScene* m_scene = nullptr;
    CircuitView* m_view = nullptr;
    lb::ComponentId m_switch = lb::kNoComponent;
    lb::ComponentId m_gate = lb::kNoComponent;
    lb::ComponentId m_led = lb::kNoComponent;
};

void GuiSmokeTest::init()
{
    m_document = new CircuitDocument;
    m_scene = new CircuitScene(m_document);

    // The view sits under a caption strip. In CI the caption is just a label
    // nobody reads; in a recording it is what makes the run legible.
    m_window = new QWidget;
    m_window->setWindowTitle(QStringLiteral("LogicBench — GUI smoke test"));
    auto* layout = new QVBoxLayout(m_window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_narration = new QLabel(m_window);
    m_narration->setMargin(10);
    m_narration->setStyleSheet(QStringLiteral(
        "background:#1F2733; color:#F2F5F9; font-size:15px; font-weight:600;"));
    layout->addWidget(m_narration);

    m_view = new CircuitView(m_scene, m_window);
    layout->addWidget(m_view, 1);

    placeForDemo(m_window);
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window));
    startCapture(m_window);
    narrate(QString::fromLatin1(QTest::currentTestFunction()));

    auto input = std::make_unique<lb::Switch>();
    input->setLabel("A");
    m_switch = m_document->addComponent(std::move(input), QPointF(-300, -100));
    m_gate = m_document->addComponent(std::make_unique<lb::NotGate>(), QPointF(-100, -100));
    auto lamp = std::make_unique<lb::Led>();
    lamp->setLabel("Y");
    m_led = m_document->addComponent(std::move(lamp), QPointF(100, -100));

    m_document->undoStack()->clear();
    m_view->frameCircuit();
    QCoreApplication::processEvents();
}

void GuiSmokeTest::cleanup()
{
    settleFrames(demoDelayMs());
    stopCapture();
    delete m_window;          // owns the narration label and the view
    m_window = nullptr;
    m_narration = nullptr;
    m_view = nullptr;
    m_scene = nullptr;
    delete m_document;
    m_document = nullptr;
}

void GuiSmokeTest::placeForDemo(QWidget* widget)
{
    widget->resize(kDemoWindowW, kDemoWindowH);
    if (!demoMode())
        return;
    widget->move(kDemoWindowX, kDemoWindowY);
    widget->raise();
    widget->activateWindow();
}

void GuiSmokeTest::startCapture(QWidget* target)
{
    m_captureTarget = target;
    if (!capturingFrames())
        return;

    m_captureDir = framesDirectory() + QLatin1Char('/')
                 + QString::fromLatin1(QTest::currentTestFunction());
    QDir().mkpath(m_captureDir);
    m_frameNumber = 0;

    if (!m_captureTimer) {
        m_captureTimer = new QTimer(this);
        connect(m_captureTimer, &QTimer::timeout, this, &GuiSmokeTest::captureFrame);
    }
    m_captureTimer->start(kFrameIntervalMs);
}

void GuiSmokeTest::stopCapture()
{
    if (m_captureTimer)
        m_captureTimer->stop();
    m_captureTarget.clear();
}

void GuiSmokeTest::captureFrame()
{
    if (m_captureTarget.isNull() || !m_captureTarget->isVisible())
        return;
    const QPixmap frame = m_captureTarget->grab();
    frame.save(QStringLiteral("%1/frame_%2.png")
                   .arg(m_captureDir, QString::number(++m_frameNumber).rightJustified(4, '0')));
}

void GuiSmokeTest::narrate(const QString& text)
{
    // The caption belongs to the canvas fixture; the whole-window tests have
    // the application's own status bar instead, so this is a no-op there.
    if (m_narration && m_window && m_window->isVisible())
        m_narration->setText(text);
    settleFrames(demoDelayMs());
}

void GuiSmokeTest::settleFrames(int ms)
{
    // qWait runs the event loop, so the window actually repaints between
    // steps instead of showing only the final state.
    QTest::qWait(ms);
    QCoreApplication::processEvents();
}

QPoint GuiSmokeTest::viewPointOfPin(lb::ComponentId id, int pin) const
{
    ComponentItem* item = m_scene->itemFor(id);
    Q_ASSERT(item);
    return m_view->mapFromScene(item->mapToScene(item->pinPosition(pin)));
}

void GuiSmokeTest::dragBetweenPins(lb::ComponentId fromId, int fromPin,
                                   lb::ComponentId toId, int toPin)
{
    const QPoint start = viewPointOfPin(fromId, fromPin);
    const QPoint end = viewPointOfPin(toId, toPin);

    QTest::mousePress(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, start);

    // One hop is enough to exercise the code; a recording wants to see the
    // rubber-band wire travel.
    const int steps = demoMode() ? 12 : 2;
    for (int i = 1; i <= steps; ++i) {
        QTest::mouseMove(m_view->viewport(), start + (end - start) * i / steps);
        settleFrames(demoMode() ? demoDelayMs() / 8 : 0);
    }

    QTest::mouseRelease(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    settleFrames(demoDelayMs() / 2);
}

void GuiSmokeTest::wiringByDragCreatesAConnection()
{
    QCOMPARE(m_document->circuit().wires().size(), std::size_t{0});

    narrate(QStringLiteral("Wiring: drag from the switch output to the NOT gate input"));
    dragBetweenPins(m_switch, 0, m_gate, 1);

    QCOMPARE(m_document->circuit().wires().size(), std::size_t{1});
    const lb::Wire& wire = m_document->circuit().wires().front();
    QCOMPARE(wire.from.component, m_switch);
    QCOMPARE(wire.to.component, m_gate);

    // And it is undoable, like every other edit.
    narrate(QStringLiteral("Undo removes the wire…"));
    m_document->undoStack()->undo();
    settleFrames(demoDelayMs());
    QCOMPARE(m_document->circuit().wires().size(), std::size_t{0});
    narrate(QStringLiteral("…and redo puts it back"));
    m_document->undoStack()->redo();
    settleFrames(demoDelayMs());
    QCOMPARE(m_document->circuit().wires().size(), std::size_t{1});
}

void GuiSmokeTest::wiringBackwardsIsAcceptedAndNormalised()
{
    // Dragging from the load to the driver is the same connection; the engine
    // only ever stores output to input.
    narrate(QStringLiteral("Wiring backwards: gate input → switch output, same wire"));
    dragBetweenPins(m_gate, 1, m_switch, 0);

    QCOMPARE(m_document->circuit().wires().size(), std::size_t{1});
    const lb::Wire& wire = m_document->circuit().wires().front();
    QCOMPARE(wire.from.component, m_switch);
    QCOMPARE(wire.from.pin, 0);
    QCOMPARE(wire.to.component, m_gate);
}

void GuiSmokeTest::anIllegalConnectionIsRefusedWithAMessage()
{
    QSignalSpy messages(m_scene, &CircuitScene::statusMessage);

    // Output to output: the switch drives, and so does the gate.
    narrate(QStringLiteral("Illegal: output to output — expect a refusal in the status line"));
    dragBetweenPins(m_switch, 0, m_gate, 0);

    QCOMPARE(m_document->circuit().wires().size(), std::size_t{0});
    QVERIFY(!messages.isEmpty());
    const QString last = messages.last().first().toString();
    narrate(last);
    QVERIFY2(last.contains(QStringLiteral("Cannot connect")), qPrintable(last));
}

void GuiSmokeTest::clickingASwitchTogglesItAndTheLedFollows()
{
    narrate(QStringLiteral("Building switch → NOT → LED"));
    dragBetweenPins(m_switch, 0, m_gate, 1);
    dragBetweenPins(m_gate, 0, m_led, 0);
    m_document->resimulate();
    m_document->undoStack()->clear();   // the wiring above is undoable; the poke should not be

    const auto ledValue = [this] { return level(m_document->circuit().component(m_led), 0); };
    QCOMPARE(ledValue(), '1');       // NOT of a low switch

    narrate(QStringLiteral("LED reads 1. Clicking the switch should flip it to 0"));
    ComponentItem* item = m_scene->itemFor(m_switch);
    const QPoint centre = m_view->mapFromScene(item->mapToScene(QPointF(28, 17)));
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, centre);
    settleFrames(demoDelayMs());

    QCOMPARE(ledValue(), '0');

    // A poke is an input, not an edit: it must not fill the undo stack.
    QCOMPARE(m_document->undoStack()->count(), 0);
}

void GuiSmokeTest::draggingAComponentIsOneUndoStep()
{
    narrate(QStringLiteral("Dragging the gate — a whole drag is one undo step"));
    ComponentItem* item = m_scene->itemFor(m_gate);
    const QPointF before = item->pos();
    const QPoint start = m_view->mapFromScene(item->mapToScene(QPointF(28, 17)));
    const QPoint end = start + QPoint(60, 40);

    QTest::mousePress(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    const int steps = demoMode() ? 14 : 6;
    for (int step = 1; step <= steps; ++step) {
        QTest::mouseMove(m_view->viewport(), start + (end - start) * step / steps);
        settleFrames(demoMode() ? demoDelayMs() / 8 : 0);
    }
    QTest::mouseRelease(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    settleFrames(demoDelayMs());

    const lb::Component* moved = m_document->circuit().component(m_gate);
    QVERIFY2(QPointF(moved->x(), moved->y()) != before, "the drag should move the model");
    QCOMPARE(m_document->undoStack()->count(), 1);   // one drag, one undo step

    narrate(QStringLiteral("Undo returns it in one step"));
    m_document->undoStack()->undo();
    settleFrames(demoDelayMs());
    const lb::Component* restored = m_document->circuit().component(m_gate);
    QCOMPARE(QPointF(restored->x(), restored->y()), before);
}

void GuiSmokeTest::deletingAComponentAndUndoingRestoresItsWires()
{
    dragBetweenPins(m_switch, 0, m_gate, 1);
    dragBetweenPins(m_gate, 0, m_led, 0);
    QCOMPARE(m_document->circuit().wires().size(), std::size_t{2});

    narrate(QStringLiteral("Deleting the gate takes its two wires with it"));
    m_scene->itemFor(m_gate)->setSelected(true);
    settleFrames(demoDelayMs() / 2);
    QTest::keyClick(m_view->viewport(), Qt::Key_Delete);
    settleFrames(demoDelayMs());

    QCOMPARE(m_document->circuit().componentCount(), std::size_t{2});
    QCOMPARE(m_document->circuit().wires().size(), std::size_t{0});

    narrate(QStringLiteral("Undo restores the gate and both wires"));
    m_document->undoStack()->undo();
    settleFrames(demoDelayMs());
    QCOMPARE(m_document->circuit().componentCount(), std::size_t{3});
    QCOMPARE(m_document->circuit().wires().size(), std::size_t{2});

    // The restored gate is wired back in, so the simulation still runs through
    // it rather than around it.
    m_document->resimulate();
    QCOMPARE(level(m_document->circuit().component(m_led), 0), '1');
}

// --- whole-window tests -----------------------------------------------------
//
// These drive MainWindow rather than a bare canvas: the run timer, the docks
// and the dialogs are as much a part of the product as the editor is, and they
// are where a regression would otherwise go unnoticed until a demo.

void GuiSmokeTest::runningTheClockAdvancesTheTimingDiagram()
{
    m_window->hide();                       // one window on screen at a time

    lbgui::MainWindow window;
    placeForDemo(&window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    startCapture(&window);
    window.openFile(QStringLiteral(LOGICBENCH_EXAMPLES_DIR "/counter3.lbc"));
    window.traceAllSignals();
    settleFrames(demoDelayMs());

    lbgui::CircuitDocument* document = window.document();
    const lb::Component* bit0 = nullptr;
    for (const std::unique_ptr<lb::Component>& c : document->circuit().components()) {
        if (c->label() == "bit 0")
            bit0 = c.get();
    }
    QVERIFY(bit0 != nullptr);

    QSignalSpy ticks(document, &lbgui::CircuitDocument::ticked);

    QAction* run = window.findChild<QAction*>(QStringLiteral("runAction"));
    QVERIFY(run != nullptr);
    run->trigger();                         // same path as the toolbar button

    // Long enough for the clock to drive several counts at the default tick
    // interval of 40 ms.
    narrate(QStringLiteral("Run: the clock drives the counter and the timing diagram records it"));
    QTest::qWait(demoMode() ? 6000 : 1200);
    run->trigger();                         // pause

    QVERIFY2(ticks.count() > 10, qPrintable(QStringLiteral("ticks: %1").arg(ticks.count())));

    // The counter has to have actually counted, not just ticked.
    const QList<QVariant> lastTick = ticks.last();
    QVERIFY(lastTick.first().toULongLong() > 10);
    QVERIFY2(bit0->pin(0).signal() != lb::Signal::Undefined,
             "the low bit should be driven once the clock has run");
}

void GuiSmokeTest::theTruthTableDialogFillsFromTheCircuit()
{
    m_window->hide();

    lbgui::MainWindow window;
    placeForDemo(&window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    startCapture(&window);
    window.openFile(QStringLiteral(LOGICBENCH_EXAMPLES_DIR "/full_adder.lbc"));
    settleFrames(demoDelayMs());

    // Shown rather than exec()d: a modal loop would never hand control back.
    lbgui::TruthTableDialog dialog(window.document(), &window);
    dialog.resize(720, 460);
    if (demoMode())
        dialog.move(kDemoWindowX + 120, kDemoWindowY + 80);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    m_captureTarget = &dialog;              // the dialog is its own window
    settleFrames(demoDelayMs() * 3);

    auto* table = dialog.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 8);         // three inputs
    QCOMPARE(table->columnCount(), 5);      // A, B, Cin, S, Cout

    // Spot-check the row that carries: 1 + 1 + 1 = 1 with a carry out.
    QCOMPARE(table->item(7, 0)->text(), QStringLiteral("1"));
    QCOMPARE(table->item(7, 1)->text(), QStringLiteral("1"));
    QCOMPARE(table->item(7, 2)->text(), QStringLiteral("1"));
    QCOMPARE(table->item(7, 3)->text(), QStringLiteral("1"));
    QCOMPARE(table->item(7, 4)->text(), QStringLiteral("1"));

    // And the row that does not: 0 + 0 + 0.
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("0"));
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("0"));
}

QTEST_MAIN(GuiSmokeTest)
#include "GuiSmokeTest.moc"
