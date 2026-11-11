#pragma once

#include <QObject>
#include <QString>

#include <array>

namespace lbgui {

class CircuitDocument;

/// The six-step half-adder tutorial (FR-3): watches the open circuit and ticks
/// each step off as the user does it, in any order. The Quick start tab shows
/// the ticks; the usability test (NFR-1) is whether an outsider gets to six.
class QuickStartTracker : public QObject {
    Q_OBJECT

public:
    static constexpr int kSteps = 6;

    explicit QuickStartTracker(CircuitDocument* document, QObject* parent = nullptr);

    static QString stepText(int i);
    bool isDone(int i) const;
    int doneCount() const;
    void reset();

    /// The truth table was opened; step 6 needs this as well as the circuit.
    void markTruthTableOpened();

signals:
    /// A step has just been completed (0-based).
    void progressed(int step);

private slots:
    void evaluate();

private:
    CircuitDocument* m_document;
    std::array<bool, kSteps> m_done{};
    bool m_truthTableOpened = false;
};

} // namespace lbgui
