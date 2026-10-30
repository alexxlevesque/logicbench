#include "CircuitScene.h"

#include "CircuitDocument.h"
#include "ComponentItem.h"
#include "Theme.h"
#include "WireItem.h"

#include "logicbench/CircuitSerializer.h"
#include "logicbench/ComponentFactory.h"
#include "logicbench/SubCircuit.h"

#include <QFileInfo>
#include <QGraphicsPathItem>
#include <QScrollBar>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QWheelEvent>

namespace lbgui {

std::unique_ptr<lb::Component> CircuitScene::createFromPayload(const QString& payload)
{
    static const QString kBlockPrefix = QStringLiteral("@block:");
    if (!payload.startsWith(kBlockPrefix))
        return lb::ComponentFactory::standard().create(payload.toStdString());

    const QString path = payload.mid(kBlockPrefix.size());
    std::unique_ptr<lb::Circuit> inner = lb::CircuitSerializer::load(path.toStdString());
    const QString name = QFileInfo(path).completeBaseName();
    return std::make_unique<lb::SubCircuit>(name.toStdString(), std::move(inner));
}

CircuitScene::CircuitScene(CircuitDocument* document, QObject* parent)
    : QGraphicsScene(parent)
    , m_document(document)
{
    setSceneRect(-2000, -2000, 4000, 4000);
    setBackgroundBrush(theme::canvasColor());

    connect(m_document, &CircuitDocument::structureChanged, this, &CircuitScene::rebuild);
    connect(m_document, &CircuitDocument::signalsChanged, this, &CircuitScene::refreshSignals);
    connect(m_document, &CircuitDocument::geometryChanged, this, &CircuitScene::syncGeometry);
    connect(this, &QGraphicsScene::selectionChanged, this, [this] {
        const QList<lb::ComponentId> selected = selectedComponentIds();
        if (selected.size() == 1) {
            const lb::Component* c = m_document->circuit().component(selected.first());
            emit selectionDescriptionChanged(c ? QString::fromStdString(c->typeName()) : QString());
        } else {
            emit selectionDescriptionChanged(QString());
        }
    });

    rebuild();
}

void CircuitScene::rebuild()
{
    if (m_rebuilding)
        return;
    m_rebuilding = true;

    const QList<lb::ComponentId> previouslySelected = selectedComponentIds();

    cancelWire();
    m_components.clear();
    m_wires.clear();
    clear();                       // destroys every item

    for (const std::unique_ptr<lb::Component>& component : m_document->circuit().components()) {
        auto* item = new ComponentItem(this, component->id());
        item->setPos(component->x(), component->y());
        addItem(item);
        m_components.insert(component->id(), item);
    }

    for (const lb::Wire& wire : m_document->circuit().wires()) {
        auto* item = new WireItem(this, wire.id);
        addItem(item);
        m_wires.insert(wire.id, item);
    }

    for (lb::ComponentId id : previouslySelected) {
        if (ComponentItem* item = m_components.value(id, nullptr))
            item->setSelected(true);
    }

    m_rebuilding = false;
    update();
}

void CircuitScene::refreshSignals()
{
    for (ComponentItem* item : std::as_const(m_components))
        item->update();
    for (WireItem* item : std::as_const(m_wires))
        item->update();
}

void CircuitScene::syncGeometry()
{
    for (auto it = m_components.constBegin(); it != m_components.constEnd(); ++it) {
        const lb::Component* component = m_document->circuit().component(it.key());
        if (!component)
            continue;
        const QPointF wanted(component->x(), component->y());
        if (it.value()->pos() != wanted)
            it.value()->setPos(wanted);
    }
    for (WireItem* wire : std::as_const(m_wires))
        wire->refresh();
    update();
}

QList<lb::ComponentId> CircuitScene::selectedComponentIds() const
{
    QList<lb::ComponentId> ids;
    for (QGraphicsItem* item : selectedItems()) {
        if (auto* component = qgraphicsitem_cast<ComponentItem*>(item))
            ids.append(component->componentId());
    }
    return ids;
}

void CircuitScene::deleteSelection()
{
    const QList<lb::ComponentId> components = selectedComponentIds();

    QList<lb::WireId> wires;
    for (QGraphicsItem* item : selectedItems()) {
        if (auto* wire = qgraphicsitem_cast<WireItem*>(item))
            wires.append(wire->wireId());
    }

    if (!components.isEmpty()) {
        m_document->deleteComponents(components);
    } else {
        for (lb::WireId id : wires)
            m_document->disconnectWire(id);
    }
}

// --------------------------------------------------------------- wiring ----

void CircuitScene::beginWire(ComponentItem* item, int pin)
{
    cancelWire();
    m_wireSource = item;
    m_wireSourcePin = pin;

    m_wirePreview = new QGraphicsPathItem;
    QPen pen(theme::selectionColor(), 2.0, Qt::DashLine);
    m_wirePreview->setPen(pen);
    m_wirePreview->setZValue(10.0);
    addItem(m_wirePreview);

    emit statusMessage(tr("Drag to a pin on another component to connect."));
}

void CircuitScene::cancelWire()
{
    if (m_wirePreview) {
        removeItem(m_wirePreview);
        delete m_wirePreview;
        m_wirePreview = nullptr;
    }
    m_wireSource = nullptr;
    m_wireSourcePin = -1;
}

void CircuitScene::finishWire(QPointF scenePosition)
{
    ComponentItem* source = m_wireSource;
    const int sourcePin = m_wireSourcePin;
    cancelWire();
    if (!source)
        return;

    // Ask every component whether the release landed on one of its pins,
    // rather than asking the scene which item is under the point: a pin sits
    // on the very edge of its item's shape, and a release a pixel outside it
    // should still connect.
    ComponentItem* target = nullptr;
    int targetPin = -1;
    for (ComponentItem* candidate : std::as_const(m_components)) {
        const int pin = candidate->pinAt(candidate->mapFromScene(scenePosition));
        if (pin >= 0) {
            target = candidate;
            targetPin = pin;
            break;
        }
    }

    if (!target) {
        emit statusMessage(tr("Wires must end on a pin."));
        return;
    }

    const lb::Circuit& circuit = m_document->circuit();
    const lb::Component* sourceComponent = circuit.component(source->componentId());
    const lb::Component* targetComponent = circuit.component(target->componentId());
    if (!sourceComponent || !targetComponent)
        return;

    // Let the user draw in either direction; the engine only stores
    // output-to-input.
    lb::PinRef from{source->componentId(), sourcePin};
    lb::PinRef to{target->componentId(), targetPin};
    if (sourceComponent->pin(sourcePin).isInput())
        std::swap(from, to);

    QString error;
    if (!m_document->connectPins(from, to, &error))
        emit statusMessage(tr("Cannot connect: %1").arg(error));
    else
        emit statusMessage(tr("Connected."));
}

void CircuitScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_wirePreview && m_wireSource) {
        const QPointF start =
            m_wireSource->mapToScene(m_wireSource->pinPosition(m_wireSourcePin));
        QPainterPath path(start);
        path.lineTo(event->scenePos().x(), start.y());
        path.lineTo(event->scenePos());
        m_wirePreview->setPath(path);
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void CircuitScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_wireSource) {
        finishWire(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

// ----------------------------------------------------------- moving --------

void CircuitScene::beginMove()
{
    m_moveOrigins.clear();
    for (lb::ComponentId id : selectedComponentIds()) {
        if (ComponentItem* item = m_components.value(id, nullptr))
            m_moveOrigins.insert(id, item->pos());
    }
}

void CircuitScene::componentItemMoved(ComponentItem* item)
{
    // While a drag is in progress only the wires need to follow; the model is
    // updated once, on release, so undo takes back the whole drag.
    refreshWiresFor(item->componentId());
}

void CircuitScene::endMove()
{
    if (m_moveOrigins.isEmpty())
        return;

    QList<lb::ComponentId> moved;
    QPointF delta;
    for (auto it = m_moveOrigins.constBegin(); it != m_moveOrigins.constEnd(); ++it) {
        ComponentItem* item = m_components.value(it.key(), nullptr);
        if (!item)
            continue;
        const QPointF itemDelta = item->pos() - it.value();
        if (!itemDelta.isNull()) {
            moved.append(it.key());
            delta = itemDelta;
        }
    }
    m_moveOrigins.clear();

    if (!moved.isEmpty())
        m_document->moveComponents(moved, delta);
}

void CircuitScene::refreshWiresFor(lb::ComponentId id)
{
    for (const lb::Wire& wire : m_document->circuit().wiresAt(id)) {
        if (WireItem* item = m_wires.value(wire.id, nullptr))
            item->refresh();
    }
}

// ------------------------------------------------------ keys and drops -----

void CircuitScene::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_wireSource) {
        cancelWire();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void CircuitScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat(componentMimeType()))
        event->acceptProposedAction();
    else
        QGraphicsScene::dragEnterEvent(event);
}

void CircuitScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat(componentMimeType()))
        event->acceptProposedAction();
    else
        QGraphicsScene::dragMoveEvent(event);
}

void CircuitScene::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    if (!event->mimeData()->hasFormat(componentMimeType())) {
        QGraphicsScene::dropEvent(event);
        return;
    }

    const QString typeName =
        QString::fromUtf8(event->mimeData()->data(componentMimeType()));
    try {
        std::unique_ptr<lb::Component> component = createFromPayload(typeName);
        QPointF position = event->scenePos();
        position.setX(qRound(position.x() / theme::kGrid) * theme::kGrid);
        position.setY(qRound(position.y() / theme::kGrid) * theme::kGrid);
        m_document->addComponent(std::move(component), position);
        event->acceptProposedAction();
    } catch (const std::exception& e) {
        emit statusMessage(tr("Cannot place %1: %2").arg(typeName, QString::fromUtf8(e.what())));
    }
}

void CircuitScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, theme::canvasColor());

    // A dot grid rather than lines: it marks the snap points without competing
    // with the wires for attention.
    painter->setPen(QPen(theme::gridColor(), 1.0));
    const qreal step = theme::kGrid * 2;
    const qreal left = std::floor(rect.left() / step) * step;
    const qreal top = std::floor(rect.top() / step) * step;
    QVector<QPointF> dots;
    for (qreal x = left; x < rect.right(); x += step) {
        for (qreal y = top; y < rect.bottom(); y += step)
            dots.append(QPointF(x, y));
    }
    painter->drawPoints(dots.data(), static_cast<int>(dots.size()));
}

// ------------------------------------------------------------- the view ----

CircuitView::CircuitView(CircuitScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    centerOn(0, 0);
}

void CircuitView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        applyZoom(event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void CircuitView::applyZoom(qreal factor)
{
    const qreal wanted = m_zoom * factor;
    if (wanted < 0.25 || wanted > 4.0)
        return;
    m_zoom = wanted;
    scale(factor, factor);
}

void CircuitView::resetZoom()
{
    resetTransform();
    m_zoom = 1.0;
}

void CircuitView::frameCircuit()
{
    const QRectF contents = scene()->itemsBoundingRect();
    if (contents.isEmpty()) {
        resetZoom();
        centerOn(0, 0);
        return;
    }

    const QRectF padded = contents.adjusted(-40, -40, 40, 40);
    resetTransform();
    m_zoom = 1.0;
    // Only shrink: a two-gate circuit blown up to fill the window looks
    // broken, and the grid stops meaning anything.
    const qreal scaleX = viewport()->width() / padded.width();
    const qreal scaleY = viewport()->height() / padded.height();
    const qreal wanted = std::min({scaleX, scaleY, 1.0});
    if (wanted < 1.0) {
        m_zoom = std::max(wanted, 0.25);
        scale(m_zoom, m_zoom);
    }
    centerOn(contents.center());
}

} // namespace lbgui
