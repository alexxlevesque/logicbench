#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/Simulator.h"

#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QUndoStack>

#include <memory>

namespace lbgui {

/// The application layer: one open circuit, its simulator, its undo stack and
/// the file it came from.
///
/// Every edit the user makes arrives here as a QUndoCommand, so undo is not a
/// feature bolted on at the end but the only way the circuit is ever changed.
/// The view holds no state of its own beyond geometry; it reads the document
/// and redraws when the document says something moved.
class CircuitDocument : public QObject {
    Q_OBJECT

public:
    explicit CircuitDocument(QObject* parent = nullptr);
    ~CircuitDocument() override;

    lb::Circuit& circuit() { return *m_circuit; }
    const lb::Circuit& circuit() const { return *m_circuit; }
    lb::Simulator& simulator() { return *m_simulator; }

    QUndoStack* undoStack() { return &m_undoStack; }

    QString filePath() const { return m_filePath; }
    QString displayName() const;
    bool isModified() const { return !m_undoStack.isClean(); }

    // ---- edits, each one undoable -----------------------------------------
    lb::ComponentId addComponent(std::unique_ptr<lb::Component> component, QPointF position);
    void deleteComponents(const QList<lb::ComponentId>& ids);
    void moveComponents(const QList<lb::ComponentId>& ids, QPointF delta);
    bool connectPins(lb::PinRef from, lb::PinRef to, QString* error = nullptr);
    void disconnectWire(lb::WireId id);
    void setComponentProperty(lb::ComponentId id, const QString& name, const QString& value);

    /// Adds a copy of `fragment`, moved by `offset`, as one undo step (paste,
    /// duplicate, build from expression). Returns the new components' ids.
    QList<lb::ComponentId> insertFragment(const lb::Circuit& fragment, QPointF offset,
                                          const QString& description);

    /// Parses the equations and builds their circuit (one switch per input,
    /// one LED per output), as one undo step. intoOpen adds it to the right of
    /// what is already on the canvas; otherwise it replaces the canvas with a
    /// new untitled circuit. On a syntax error nothing changes and `error`
    /// gives the line and column.
    bool buildFromEquations(const QString& text, bool intoOpen, QString* error = nullptr);

    /// Flipping a switch is deliberately not undoable: it is an input to the
    /// simulation, not an edit to the circuit, and filling the undo stack with
    /// clicks would bury the edits the user actually wants back.
    void toggleSwitch(lb::ComponentId id);

    // ---- file -------------------------------------------------------------
    void newCircuit();
    bool open(const QString& path, QString* error);
    bool save(const QString& path, QString* error);

    /// Opens a file as a new untitled circuit (the bundled examples): the
    /// original is never written by a later Save.
    bool openCopy(const QString& path, QString* error);

    /// Loads the autosaved recovery file. The circuit takes the original's
    /// path (empty if it was untitled) and counts as unsaved, so Save writes
    /// it back where it belongs and Quit still asks.
    bool restoreRecovery(const QString& recoveryPath, const QString& originalPath, QString* error);

    // ---- simulation -------------------------------------------------------
    void resimulate();
    void advance(lb::Tick ticks);
    QList<lb::ComponentId> oscillating() const { return m_oscillating; }
    bool isOscillating(lb::ComponentId id) const { return m_oscillating.contains(id); }

    // ---- low-level operations used by the undo commands -------------------
    lb::ComponentId insertComponent(std::unique_ptr<lb::Component> component,
                                    lb::ComponentId preferredId);
    std::unique_ptr<lb::Component> extractComponent(lb::ComponentId id);
    lb::WireId insertWire(lb::PinRef from, lb::PinRef to, lb::WireId preferredId);
    void removeWire(lb::WireId id);
    void offsetComponent(lb::ComponentId id, QPointF delta);
    void applyProperty(lb::ComponentId id, const QString& name, const QString& value);
    lb::FragmentIds insertFragmentIds(const lb::Circuit& fragment, QPointF offset,
                                      const lb::FragmentIds& preferred);
    void removeFragment(const lb::FragmentIds& ids);
    void notifyStructureChanged();
    void notifyGeometryChanged();

signals:
    /// Components or wires were added, removed or resized: the scene rebuilds.
    void structureChanged();
    /// Only signal values changed: the scene repaints.
    void signalsChanged();
    /// Only positions changed. Kept separate from structureChanged because a
    /// rebuild would destroy the very item whose drag is being handled.
    void geometryChanged();
    /// One simulated tick completed; the timing diagram samples on this.
    void ticked(quint64 now);
    void statusMessage(const QString& text);

private:
    void rebuildSimulator();

    std::unique_ptr<lb::Circuit> m_circuit;
    std::unique_ptr<lb::Simulator> m_simulator;
    QUndoStack m_undoStack;
    QString m_filePath;
    QList<lb::ComponentId> m_oscillating;
};

} // namespace lbgui
