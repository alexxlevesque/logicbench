#include "WireItem.h"

#include "CircuitDocument.h"
#include "CircuitScene.h"
#include "ComponentItem.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOptionGraphicsItem>

namespace lbgui {

WireItem::WireItem(CircuitScene* scene, lb::WireId id)
    : m_scene(scene)
    , m_id(id)
{
    setFlags(ItemIsSelectable);
    setZValue(-1.0);            // wires pass under components
    refresh();
}

void WireItem::refresh()
{
    const lb::Wire* wire = m_scene->document()->circuit().wire(m_id);
    if (!wire)
        return;

    ComponentItem* source = m_scene->itemFor(wire->from.component);
    ComponentItem* sink = m_scene->itemFor(wire->to.component);
    if (!source || !sink)
        return;

    const QPointF start = source->mapToScene(source->pinPosition(wire->from.pin));
    const QPointF end = sink->mapToScene(sink->pinPosition(wire->to.pin));

    // Out of the driver, across at the midpoint, into the load. When the load
    // sits behind the driver the midpoint would double back, so the detour
    // goes around instead.
    QPainterPath path(start);
    const qreal midX = (start.x() + end.x()) / 2.0;
    if (end.x() >= start.x() + 2 * theme::kGrid) {
        path.lineTo(midX, start.y());
        path.lineTo(midX, end.y());
        path.lineTo(end);
    } else {
        const qreal stub = theme::kGrid * 1.5;
        const qreal detourY = (start.y() + end.y()) / 2.0
                            + (start.y() <= end.y() ? -theme::kGrid * 2.0 : theme::kGrid * 2.0);
        path.lineTo(start.x() + stub, start.y());
        path.lineTo(start.x() + stub, detourY);
        path.lineTo(end.x() - stub, detourY);
        path.lineTo(end.x() - stub, end.y());
        path.lineTo(end);
    }
    setPath(path);
}

void WireItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const lb::Wire* wire = m_scene->document()->circuit().wire(m_id);
    if (!wire)
        return;

    const lb::Signal signal = m_scene->document()->circuit().signalAt(wire->from);
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (isSelected()) {
        painter->setPen(QPen(theme::selectionColor(), 5.0, Qt::SolidLine, Qt::RoundCap,
                             Qt::RoundJoin));
        painter->drawPath(path());
    }

    QPen pen(theme::signalColor(signal), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (signal == lb::Signal::Undefined)
        pen.setStyle(Qt::DashLine);     // an undefined wire should not look driven
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    // A junction dot wherever one output feeds several inputs, so a shared
    // node is not mistaken for two wires crossing.
    const std::vector<lb::PinRef>& sinks = m_scene->document()->circuit().sinksOf(wire->from);
    if (sinks.size() > 1) {
        if (ComponentItem* source = m_scene->itemFor(wire->from.component)) {
            painter->setBrush(theme::signalColor(signal));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(source->mapToScene(source->pinPosition(wire->from.pin)), 3.2, 3.2);
        }
    }
}

} // namespace lbgui
