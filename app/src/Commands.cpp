#include "Commands.h"

#include "CircuitDocument.h"

#include "logicbench/Circuit.h"

#include <QObject>

#include <algorithm>

namespace lbgui {

namespace {

QString describe(const lb::Component& component)
{
    const QString type = QString::fromStdString(component.typeName());
    return component.label().empty()
               ? type
               : QStringLiteral("%1 \"%2\"").arg(type, QString::fromStdString(component.label()));
}

} // namespace

// ------------------------------------------------------- add a component ---

AddComponentCommand::AddComponentCommand(CircuitDocument* document,
                                         std::unique_ptr<lb::Component> component)
    : m_document(document)
    , m_stored(std::move(component))
{
    setText(QObject::tr("add %1").arg(describe(*m_stored)));

    // Insert immediately so the caller can report the new id, then let the
    // first redo() do nothing rather than inserting twice.
    m_id = m_document->insertComponent(std::move(m_stored), lb::kNoComponent);
    m_document->notifyStructureChanged();
}

void AddComponentCommand::redo()
{
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_id = m_document->insertComponent(std::move(m_stored), m_id);
    m_document->notifyStructureChanged();
}

void AddComponentCommand::undo()
{
    m_stored = m_document->extractComponent(m_id);
    m_document->notifyStructureChanged();
}

// --------------------------------------------------- delete components -----

DeleteComponentsCommand::DeleteComponentsCommand(CircuitDocument* document,
                                                 const QList<lb::ComponentId>& ids)
    : m_document(document)
    , m_ids(ids)
{
    setText(QObject::tr("delete %n component(s)", nullptr, static_cast<int>(ids.size())));

    // Record the wires before anything is removed: deleting a component takes
    // its wires with it, and undo has to put both back.
    const lb::Circuit& circuit = m_document->circuit();
    for (lb::ComponentId id : m_ids) {
        for (const lb::Wire& wire : circuit.wiresAt(id)) {
            const bool alreadyStored =
                std::any_of(m_wires.begin(), m_wires.end(),
                            [&wire](const lb::Wire& w) { return w.id == wire.id; });
            if (!alreadyStored)
                m_wires.push_back(wire);
        }
    }
}

void DeleteComponentsCommand::redo()
{
    m_stored.clear();
    for (lb::ComponentId id : m_ids) {
        if (std::unique_ptr<lb::Component> component = m_document->extractComponent(id))
            m_stored.push_back(std::move(component));
    }
    m_document->notifyStructureChanged();
}

void DeleteComponentsCommand::undo()
{
    for (std::unique_ptr<lb::Component>& component : m_stored) {
        const lb::ComponentId id = component->id();
        m_document->insertComponent(std::move(component), id);
    }
    m_stored.clear();

    for (const lb::Wire& wire : m_wires) {
        try {
            m_document->insertWire(wire.from, wire.to, wire.id);
        } catch (const lb::IllegalConnection&) {
            // The pin it fed may have been narrowed away by a later command
            // that has also been undone by now; skipping is correct and rare.
        }
    }
    m_document->notifyStructureChanged();
}

// ----------------------------------------------------- move components -----

MoveComponentsCommand::MoveComponentsCommand(CircuitDocument* document,
                                             const QList<lb::ComponentId>& ids, QPointF delta)
    : m_document(document)
    , m_ids(ids)
    , m_delta(delta)
{
    setText(QObject::tr("move %n component(s)", nullptr, static_cast<int>(ids.size())));
}

void MoveComponentsCommand::redo()
{
    // On the first pass the view has already moved the items and the model is
    // catching up; on a redo both have to move. Either way the scene is told
    // that geometry changed rather than structure, because rebuilding the
    // scene here would destroy the item whose drag is still being handled.
    for (lb::ComponentId id : m_ids)
        m_document->offsetComponent(id, m_delta);
    m_document->notifyGeometryChanged();
}

void MoveComponentsCommand::undo()
{
    for (lb::ComponentId id : m_ids)
        m_document->offsetComponent(id, -m_delta);
    m_document->notifyGeometryChanged();
}

bool MoveComponentsCommand::mergeWith(const QUndoCommand* other)
{
    const auto* move = static_cast<const MoveComponentsCommand*>(other);
    if (move->m_ids != m_ids)
        return false;
    // A drag arrives as a stream of small moves; one undo should take the
    // whole drag back, not one pixel of it.
    m_delta += move->m_delta;
    return true;
}

// ------------------------------------------------------------ wiring -------

ConnectCommand::ConnectCommand(CircuitDocument* document, lb::PinRef from, lb::PinRef to)
    : m_document(document)
    , m_from(from)
    , m_to(to)
{
    setText(QObject::tr("connect"));
}

void ConnectCommand::redo()
{
    m_wire = m_document->insertWire(m_from, m_to, m_wire);
    m_document->notifyStructureChanged();
}

void ConnectCommand::undo()
{
    m_document->removeWire(m_wire);
    m_document->notifyStructureChanged();
}

DisconnectCommand::DisconnectCommand(CircuitDocument* document, const lb::Wire& wire)
    : m_document(document)
    , m_wire(wire)
{
    setText(QObject::tr("disconnect"));
}

void DisconnectCommand::redo()
{
    m_document->removeWire(m_wire.id);
    m_document->notifyStructureChanged();
}

void DisconnectCommand::undo()
{
    m_document->insertWire(m_wire.from, m_wire.to, m_wire.id);
    m_document->notifyStructureChanged();
}

// ---------------------------------------------------------- properties -----

SetPropertyCommand::SetPropertyCommand(CircuitDocument* document, lb::ComponentId id,
                                       QString name, QString before, QString after)
    : m_document(document)
    , m_id(id)
    , m_name(std::move(name))
    , m_before(std::move(before))
    , m_after(std::move(after))
{
    setText(QObject::tr("change %1").arg(m_name));
}

void SetPropertyCommand::redo()
{
    // Narrowing a gate destroys wires. Remember them so undo restores the
    // connections along with the pin count.
    m_removedWires.clear();
    if (m_name == QLatin1String("inputs")) {
        const lb::Circuit& circuit = m_document->circuit();
        if (const lb::Component* component = circuit.component(m_id)) {
            const std::size_t after = static_cast<std::size_t>(m_after.toInt()) + 1;
            for (const lb::Wire& wire : circuit.wiresAt(m_id)) {
                if (wire.to.component == m_id && static_cast<std::size_t>(wire.to.pin) >= after)
                    m_removedWires.push_back(wire);
            }
            (void) component;
        }
    }

    m_document->applyProperty(m_id, m_name, m_after);
    m_document->notifyStructureChanged();
}

void SetPropertyCommand::undo()
{
    m_document->applyProperty(m_id, m_name, m_before);
    for (const lb::Wire& wire : m_removedWires) {
        try {
            m_document->insertWire(wire.from, wire.to, wire.id);
        } catch (const lb::IllegalConnection&) {
        }
    }
    m_document->notifyStructureChanged();
}

} // namespace lbgui
