#pragma once

#include "logicbench/Pin.h"
#include "logicbench/Signal.h"

#include <QDockWidget>
#include <QHash>
#include <QList>
#include <QVector>
#include <QWidget>

class QListWidget;
class QListWidgetItem;

namespace lbgui {

class CircuitDocument;

/// Paints the recorded traces. Kept separate from the dock so the drawing
/// code has one job and can be looked at on its own.
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    struct Trace {
        lb::ComponentId component = lb::kNoComponent;
        int pin = 0;
        QString name;
        QVector<lb::Signal> samples;
    };

    explicit WaveformWidget(QWidget* parent = nullptr);

    void setTraces(QVector<Trace> traces);
    QVector<Trace>& traces() { return m_traces; }
    void clearSamples();

    QSize sizeHint() const override { return QSize(600, 160); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<Trace> m_traces;
};

/// The timing diagram: pick signals on the left, watch them on the right.
///
/// Samples are taken on the document's tick signal, so the trace is the
/// simulator's own history rather than a repaint-time guess.
class WaveformDock : public QDockWidget {
    Q_OBJECT

public:
    /// Samples kept per trace. At 20 ms a tick this is about a minute of
    /// history, which is longer than anyone watches a counter for.
    static constexpr int kHistory = 3000;

    explicit WaveformDock(CircuitDocument* document, QWidget* parent = nullptr);

    /// The drawing itself, for Export image.
    WaveformWidget* waveformWidget() const { return m_waveforms; }

public slots:
    /// Rebuilds the list of signals that can be plotted.
    void refreshSignalList();
    void clear();
    /// Ticks every signal in the list. Used by the --screenshot path and by
    /// anyone who wants the whole circuit on the diagram at once.
    void selectAllSignals();

private slots:
    void sample();

private:
    void rebuildTraces();

    CircuitDocument* m_document;
    QListWidget* m_signalList;
    WaveformWidget* m_waveforms;
    QHash<QString, bool> m_checkedByKey;   ///< survives a rebuild of the list
};

} // namespace lbgui
