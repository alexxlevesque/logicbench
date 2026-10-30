#pragma once

#include "logicbench/Component.h"

#include <QGraphicsObject>
#include <QPointF>

namespace lbgui {

class CircuitScene;

/// One component drawn on the canvas.
///
/// The item owns no logic: it asks the engine for the component's pins and
/// signals every time it paints. Anything else drifts out of step with the
/// simulation the first time an undo touches the circuit.
class ComponentItem : public QGraphicsObject {
    Q_OBJECT

public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    ComponentItem(CircuitScene* scene, lb::ComponentId id);

    lb::ComponentId componentId() const { return m_id; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    /// Where a pin sits, in this item's coordinates.
    QPointF pinPosition(int pinIndex) const;
    /// Pin under a point in item coordinates, or -1.
    int pinAt(QPointF localPosition) const;

    /// Recomputes the body size after the pin list changes.
    void refreshGeometry();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    const lb::Component* component() const;

    void paintGate(QPainter* painter, const QString& type);
    void paintSwitch(QPainter* painter);
    void paintLed(QPainter* painter);
    void paintClock(QPainter* painter);
    void paintFlipFlop(QPainter* painter);
    void paintBlock(QPainter* painter);
    void paintPins(QPainter* painter);

    CircuitScene* m_scene;
    lb::ComponentId m_id;
    QRectF m_body;
    int m_hoverPin = -1;
    QPointF m_pressScenePos;
    bool m_pressedOnBody = false;
};

} // namespace lbgui
