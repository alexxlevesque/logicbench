#pragma once

#include "logicbench/Component.h"
#include "logicbench/Pin.h"

#include <QList>
#include <QPointF>
#include <QString>
#include <QUndoCommand>

#include <memory>
#include <vector>

namespace lbgui {

class CircuitDocument;

/// Undoable edits. Each one owns whatever it needs to put the circuit back
/// exactly as it was, including the ids — a restored component keeps its old
/// id so the wires that referred to it still resolve.

class AddComponentCommand : public QUndoCommand {
public:
    AddComponentCommand(CircuitDocument* document,
                        std::unique_ptr<lb::Component> component);

    void undo() override;
    void redo() override;

    lb::ComponentId componentId() const { return m_id; }

private:
    CircuitDocument* m_document;
    std::unique_ptr<lb::Component> m_stored;   ///< held while the component is out of the circuit
    lb::ComponentId m_id = lb::kNoComponent;
    bool m_firstRedo = true;
};

class DeleteComponentsCommand : public QUndoCommand {
public:
    DeleteComponentsCommand(CircuitDocument* document, const QList<lb::ComponentId>& ids);

    void undo() override;
    void redo() override;

private:
    CircuitDocument* m_document;
    QList<lb::ComponentId> m_ids;
    std::vector<std::unique_ptr<lb::Component>> m_stored;
    std::vector<lb::Wire> m_wires;   ///< every wire that touched the deleted parts
};

class MoveComponentsCommand : public QUndoCommand {
public:
    MoveComponentsCommand(CircuitDocument* document, const QList<lb::ComponentId>& ids,
                          QPointF delta);

    void undo() override;
    void redo() override;

    int id() const override { return 1; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    CircuitDocument* m_document;
    QList<lb::ComponentId> m_ids;
    QPointF m_delta;
};

class ConnectCommand : public QUndoCommand {
public:
    ConnectCommand(CircuitDocument* document, lb::PinRef from, lb::PinRef to);

    void undo() override;
    void redo() override;

private:
    CircuitDocument* m_document;
    lb::PinRef m_from;
    lb::PinRef m_to;
    lb::WireId m_wire = lb::kNoWire;
};

class DisconnectCommand : public QUndoCommand {
public:
    DisconnectCommand(CircuitDocument* document, const lb::Wire& wire);

    void undo() override;
    void redo() override;

private:
    CircuitDocument* m_document;
    lb::Wire m_wire;
};

class SetPropertyCommand : public QUndoCommand {
public:
    SetPropertyCommand(CircuitDocument* document, lb::ComponentId id, QString name,
                       QString before, QString after);

    void undo() override;
    void redo() override;

private:
    CircuitDocument* m_document;
    lb::ComponentId m_id;
    QString m_name;
    QString m_before;
    QString m_after;
    std::vector<lb::Wire> m_removedWires;   ///< wires lost when a gate narrowed
};

} // namespace lbgui
