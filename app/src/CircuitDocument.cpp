#include "CircuitDocument.h"

#include "Commands.h"

#include "logicbench/CircuitSerializer.h"
#include "logicbench/IoComponents.h"

#include <QFileInfo>

namespace lbgui {

CircuitDocument::CircuitDocument(QObject* parent)
    : QObject(parent)
    , m_circuit(std::make_unique<lb::Circuit>())
    , m_simulator(std::make_unique<lb::Simulator>(*m_circuit))
{
    m_undoStack.setUndoLimit(200);
}

CircuitDocument::~CircuitDocument() = default;

QString CircuitDocument::displayName() const
{
    if (m_filePath.isEmpty())
        return QStringLiteral("untitled.lbc");
    return QFileInfo(m_filePath).fileName();
}

void CircuitDocument::rebuildSimulator()
{
    m_simulator = std::make_unique<lb::Simulator>(*m_circuit);
    m_oscillating.clear();
}

// ---------------------------------------------------------------- edits ----

lb::ComponentId CircuitDocument::addComponent(std::unique_ptr<lb::Component> component,
                                              QPointF position)
{
    if (!component)
        return lb::kNoComponent;
    component->setPosition(position.x(), position.y());
    auto* command = new AddComponentCommand(this, std::move(component));
    m_undoStack.push(command);
    return command->componentId();
}

void CircuitDocument::deleteComponents(const QList<lb::ComponentId>& ids)
{
    if (ids.isEmpty())
        return;
    m_undoStack.push(new DeleteComponentsCommand(this, ids));
}

void CircuitDocument::moveComponents(const QList<lb::ComponentId>& ids, QPointF delta)
{
    if (ids.isEmpty() || delta.isNull())
        return;
    m_undoStack.push(new MoveComponentsCommand(this, ids, delta));
}

bool CircuitDocument::connectPins(lb::PinRef from, lb::PinRef to, QString* error)
{
    // Ask the engine first so an illegal connection never reaches the undo
    // stack: a command that throws on redo would corrupt the history.
    try {
        const lb::WireId probe = m_circuit->connect(from, to);
        m_circuit->disconnect(probe);
    } catch (const lb::IllegalConnection& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }

    m_undoStack.push(new ConnectCommand(this, from, to));
    return true;
}

void CircuitDocument::disconnectWire(lb::WireId id)
{
    const lb::Wire* wire = m_circuit->wire(id);
    if (!wire)
        return;
    m_undoStack.push(new DisconnectCommand(this, *wire));
}

void CircuitDocument::setComponentProperty(lb::ComponentId id, const QString& name,
                                           const QString& value)
{
    lb::Component* component = m_circuit->component(id);
    if (!component)
        return;
    const auto properties = component->properties();
    const auto it = properties.find(name.toStdString());
    const QString previous = it == properties.end() ? QString()
                                                    : QString::fromStdString(it->second);
    if (previous == value)
        return;
    m_undoStack.push(new SetPropertyCommand(this, id, name, previous, value));
}

QList<lb::ComponentId> CircuitDocument::insertFragment(const lb::Circuit& fragment, QPointF offset,
                                                       const QString& description)
{
    if (fragment.componentCount() == 0)
        return {};
    auto* command = new InsertFragmentCommand(this, fragment, offset, description);
    m_undoStack.push(command);
    QList<lb::ComponentId> ids;
    for (lb::ComponentId id : command->insertedIds().components)
        ids.append(id);
    return ids;
}

void CircuitDocument::toggleSwitch(lb::ComponentId id)
{
    auto* toggle = dynamic_cast<lb::Switch*>(m_circuit->component(id));
    if (!toggle)
        return;
    toggle->toggle();
    m_simulator->evaluateFrom(id);
    m_oscillating.clear();
    for (lb::ComponentId oscillating : m_simulator->oscillatingComponents())
        m_oscillating.append(oscillating);
    emit signalsChanged();
}

// ----------------------------------------------------------------- file ----

void CircuitDocument::newCircuit()
{
    m_circuit = std::make_unique<lb::Circuit>();
    rebuildSimulator();
    m_undoStack.clear();
    m_filePath.clear();
    emit structureChanged();
}

bool CircuitDocument::open(const QString& path, QString* error)
{
    try {
        std::unique_ptr<lb::Circuit> loaded = lb::CircuitSerializer::load(path.toStdString());
        m_circuit = std::move(loaded);
        rebuildSimulator();
        m_undoStack.clear();
        m_filePath = path;
        resimulate();
        emit structureChanged();
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

bool CircuitDocument::save(const QString& path, QString* error)
{
    try {
        lb::CircuitSerializer::save(*m_circuit, path.toStdString());
        m_filePath = path;
        m_undoStack.setClean();
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

// ----------------------------------------------------------- simulation ----

void CircuitDocument::resimulate()
{
    const lb::SettleReport report = m_simulator->reset();
    m_oscillating.clear();
    for (lb::ComponentId id : report.oscillating)
        m_oscillating.append(id);

    if (!m_oscillating.isEmpty()) {
        emit statusMessage(tr("%n component(s) could not settle — highlighted in red.",
                              nullptr, static_cast<int>(m_oscillating.size())));
    }
    emit signalsChanged();
}

void CircuitDocument::advance(lb::Tick ticks)
{
    const lb::SettleReport report = m_simulator->advance(ticks, [this](lb::Tick now) {
        emit ticked(static_cast<quint64>(now));
    });

    m_oscillating.clear();
    for (lb::ComponentId id : report.oscillating)
        m_oscillating.append(id);
    emit signalsChanged();
}

// ------------------------------------------- low level, for the commands ----

lb::ComponentId CircuitDocument::insertComponent(std::unique_ptr<lb::Component> component,
                                                 lb::ComponentId preferredId)
{
    const lb::ComponentId id = m_circuit->add(std::move(component), preferredId);
    m_simulator->refreshClocks();
    m_simulator->evaluateFrom(id);
    return id;
}

std::unique_ptr<lb::Component> CircuitDocument::extractComponent(lb::ComponentId id)
{
    std::unique_ptr<lb::Component> component = m_circuit->take(id);
    m_simulator->refreshClocks();
    m_simulator->scheduleAll();
    m_simulator->settle();
    return component;
}

lb::WireId CircuitDocument::insertWire(lb::PinRef from, lb::PinRef to, lb::WireId preferredId)
{
    const lb::WireId id = m_circuit->connect(from, to, preferredId);
    m_simulator->evaluateFrom(to.component);
    return id;
}

void CircuitDocument::removeWire(lb::WireId id)
{
    m_circuit->disconnect(id);
    m_simulator->scheduleAll();
    m_simulator->settle();
}

void CircuitDocument::offsetComponent(lb::ComponentId id, QPointF delta)
{
    if (lb::Component* component = m_circuit->component(id))
        component->setPosition(component->x() + delta.x(), component->y() + delta.y());
}

void CircuitDocument::applyProperty(lb::ComponentId id, const QString& name,
                                    const QString& value)
{
    lb::Component* component = m_circuit->component(id);
    if (!component)
        return;

    if (name == QLatin1String("inputs")) {
        // Narrowing a gate drops pins, so the circuit has to drop the wires
        // into them as well.
        m_circuit->setInputCount(id, value.toInt());
    } else {
        component->setProperty(name.toStdString(), value.toStdString());
    }
    m_simulator->refreshClocks();
    m_simulator->scheduleAll();
    m_simulator->settle();
}

lb::FragmentIds CircuitDocument::insertFragmentIds(const lb::Circuit& fragment, QPointF offset,
                                                   const lb::FragmentIds& preferred)
{
    const lb::FragmentIds ids = m_circuit->insert(fragment, offset.x(), offset.y(), preferred);
    m_simulator->refreshClocks();
    for (lb::ComponentId id : ids.components)
        m_simulator->schedule(id);
    m_simulator->settle();
    return ids;
}

void CircuitDocument::removeFragment(const lb::FragmentIds& ids)
{
    for (lb::ComponentId id : ids.components)
        m_circuit->take(id);              // takes its wires with it
    m_simulator->refreshClocks();
    m_simulator->scheduleAll();
    m_simulator->settle();
}

void CircuitDocument::notifyGeometryChanged()
{
    emit geometryChanged();
}

void CircuitDocument::notifyStructureChanged()
{
    m_oscillating.clear();
    for (lb::ComponentId id : m_simulator->oscillatingComponents())
        m_oscillating.append(id);
    emit structureChanged();
}

} // namespace lbgui
