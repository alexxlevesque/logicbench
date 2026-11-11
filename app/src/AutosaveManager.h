#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

class QWidget;

namespace lbgui {

class CircuitDocument;

/// Crash recovery (FR-3, NFR-3).
///
/// While the open circuit has unsaved edits, writes it to recovery.lbc in the
/// user's data folder every two minutes, with recovery.json beside it naming
/// the file it came from and when. A clean exit deletes both; so does saving.
/// If they are still there at the next launch, LogicBench crashed, and the
/// user is offered the work back. At most two minutes of edits are lost.
class AutosaveManager : public QObject {
    Q_OBJECT

public:
    static constexpr int kIntervalMs = 120000;

    AutosaveManager(CircuitDocument* document, QString directory, QObject* parent = nullptr);

    QString recoveryPath() const;
    bool hasRecovery() const;
    QDateTime recoveryTime() const;
    /// The file the unsaved work belonged to, or empty if it was untitled.
    QString recoverySource() const;

    /// Asks "LogicBench closed unexpectedly. Restore the unsaved work from …?"
    /// and restores or discards. Returns true if the work was restored.
    bool offerRecovery(QWidget* parent);

    void discardRecovery();

    /// Writes the recovery file now if there are unsaved edits. The timer calls
    /// this; tests and the crash path may too.
    void writeRecovery();

    QTimer* timer() { return &m_timer; }

private slots:
    void onCleanChanged(bool clean);

private:
    QString metadataPath() const;

    CircuitDocument* m_document;
    QTimer m_timer;
    QString m_directory;
};

} // namespace lbgui
