#include "WaveformDock.h"

#include "CircuitDocument.h"
#include "Theme.h"

#include "logicbench/Component.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace lbgui {

namespace {

constexpr int kRowHeight = 34;
constexpr int kLabelWidth = 96;
constexpr qreal kSampleWidth = 4.0;

QString traceKey(lb::ComponentId id, int pin)
{
    return QStringLiteral("%1:%2").arg(id).arg(pin);
}

} // namespace

// ------------------------------------------------------- the painting -----

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setAutoFillBackground(true);
    QPalette background = palette();
    background.setColor(QPalette::Window, QColor(0xFC, 0xFD, 0xFE));
    setPalette(background);
}

void WaveformWidget::setTraces(QVector<Trace> traces)
{
    m_traces = std::move(traces);
    // Grow with the number of traces so the scroll area has something to
    // scroll; otherwise the sixth signal is simply invisible.
    setMinimumHeight(std::max(120, static_cast<int>(m_traces.size()) * kRowHeight + 12));
    update();
}

void WaveformWidget::clearSamples()
{
    for (Trace& trace : m_traces)
        trace.samples.clear();
    update();
}

void WaveformWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (m_traces.isEmpty()) {
        painter.setPen(QColor(0x8A, 0x94, 0xA6));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("Tick the signals on the left, then press Run."));
        return;
    }

    const int visible = std::max(1, static_cast<int>((width() - kLabelWidth) / kSampleWidth));

    for (int row = 0; row < m_traces.size(); ++row) {
        const Trace& trace = m_traces.at(row);
        const int top = row * kRowHeight + 6;
        const int high = top + 6;
        const int low = top + kRowHeight - 14;

        painter.setPen(QColor(0xE3, 0xE8, 0xEE));
        painter.drawLine(0, top + kRowHeight - 4, width(), top + kRowHeight - 4);

        painter.setPen(theme::labelColor());
        painter.drawText(QRect(6, top, kLabelWidth - 12, kRowHeight - 10),
                         Qt::AlignLeft | Qt::AlignVCenter, trace.name);

        const int count = trace.samples.size();
        const int first = std::max(0, count - visible);
        qreal x = kLabelWidth;
        int previousY = -1;

        for (int i = first; i < count; ++i) {
            const lb::Signal value = trace.samples.at(i);
            const int y = (value == lb::Signal::High) ? high : low;

            QPen pen(theme::signalColor(value), 1.8);
            if (value == lb::Signal::Undefined) {
                // Undefined is drawn as a band between the rails so it is not
                // mistaken for a level.
                pen.setStyle(Qt::DashLine);
                painter.setPen(pen);
                painter.drawLine(QPointF(x, (high + low) / 2.0),
                                 QPointF(x + kSampleWidth, (high + low) / 2.0));
                previousY = -1;
                x += kSampleWidth;
                continue;
            }

            painter.setPen(pen);
            if (previousY >= 0 && previousY != y)
                painter.drawLine(QPointF(x, previousY), QPointF(x, y));   // the edge
            painter.drawLine(QPointF(x, y), QPointF(x + kSampleWidth, y));
            previousY = y;
            x += kSampleWidth;
        }
    }
}

// ----------------------------------------------------------- the dock -----

WaveformDock::WaveformDock(CircuitDocument* document, QWidget* parent)
    : QDockWidget(tr("Timing diagram"), parent)
    , m_document(document)
    , m_signalList(new QListWidget(this))
    , m_waveforms(new WaveformWidget(this))
{
    setObjectName(QStringLiteral("waveformDock"));

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* scroll = new QScrollArea(container);
    scroll->setWidget(m_waveforms);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* splitter = new QSplitter(Qt::Horizontal, container);
    m_signalList->setMaximumWidth(200);
    splitter->addWidget(m_signalList);
    splitter->addWidget(scroll);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    auto* buttons = new QHBoxLayout;
    auto* clearButton = new QPushButton(tr("Clear trace"), container);
    buttons->addStretch(1);
    buttons->addWidget(clearButton);
    layout->addLayout(buttons);
    setWidget(container);

    connect(clearButton, &QPushButton::clicked, this, &WaveformDock::clear);
    connect(m_document, &CircuitDocument::structureChanged, this,
            &WaveformDock::refreshSignalList);
    connect(m_document, &CircuitDocument::ticked, this, &WaveformDock::sample);
    connect(m_signalList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        m_checkedByKey[item->data(Qt::UserRole).toString()] =
            (item->checkState() == Qt::Checked);
        rebuildTraces();
    });

    refreshSignalList();
}

void WaveformDock::refreshSignalList()
{
    const QSignalBlocker blocker(m_signalList);
    m_signalList->clear();

    // Anything worth watching over time: the inputs the user flips, the
    // clocks, the lamps, and a flip-flop's Q.
    const auto addEntry = [this](const lb::Component& component, int pin, const QString& suffix) {
        const QString base = component.label().empty()
                                 ? QStringLiteral("%1 %2")
                                       .arg(QString::fromStdString(component.typeName()))
                                       .arg(component.id())
                                 : QString::fromStdString(component.label());
        const QString key = traceKey(component.id(), pin);
        auto* item = new QListWidgetItem(suffix.isEmpty() ? base : base + suffix, m_signalList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(Qt::UserRole, key);
        item->setCheckState(m_checkedByKey.value(key, false) ? Qt::Checked : Qt::Unchecked);
    };

    for (const std::unique_ptr<lb::Component>& component : m_document->circuit().components()) {
        const std::string type = component->typeName();
        if (type == "SWITCH" || type == "CLOCK")
            addEntry(*component, 0, QString());
        else if (type == "LED")
            addEntry(*component, 0, QString());
        else if (type == "DFF")
            addEntry(*component, 0, QStringLiteral(" Q"));
    }

    rebuildTraces();
}

void WaveformDock::rebuildTraces()
{
    // Keep the samples already recorded for traces that are still selected, so
    // ticking a second signal does not wipe the first one's history.
    QHash<QString, QVector<lb::Signal>> existing;
    for (const WaveformWidget::Trace& trace : m_waveforms->traces())
        existing.insert(traceKey(trace.component, trace.pin), trace.samples);

    QVector<WaveformWidget::Trace> traces;
    for (int row = 0; row < m_signalList->count(); ++row) {
        QListWidgetItem* item = m_signalList->item(row);
        if (item->checkState() != Qt::Checked)
            continue;

        const QString key = item->data(Qt::UserRole).toString();
        const QStringList parts = key.split(QLatin1Char(':'));
        WaveformWidget::Trace trace;
        trace.component = parts.value(0).toUInt();
        trace.pin = parts.value(1).toInt();
        trace.name = item->text();
        trace.samples = existing.value(key);
        traces.append(trace);
    }
    m_waveforms->setTraces(std::move(traces));
}

void WaveformDock::sample()
{
    QVector<WaveformWidget::Trace>& traces = m_waveforms->traces();
    if (traces.isEmpty())
        return;

    for (WaveformWidget::Trace& trace : traces) {
        const lb::Component* component = m_document->circuit().component(trace.component);
        const lb::Signal value = component ? component->pin(trace.pin).signal()
                                           : lb::Signal::Undefined;
        trace.samples.append(value);
        if (trace.samples.size() > kHistory)
            trace.samples.removeFirst();
    }
    m_waveforms->update();
}

void WaveformDock::selectAllSignals()
{
    for (int row = 0; row < m_signalList->count(); ++row)
        m_signalList->item(row)->setCheckState(Qt::Checked);
}

void WaveformDock::clear()
{
    m_waveforms->clearSamples();
}

} // namespace lbgui
