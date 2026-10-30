#pragma once

#include "logicbench/Pin.h"

#include <QGraphicsPathItem>

namespace lbgui {

class CircuitScene;

/// One wire, drawn as right angles on the grid.
///
/// There is no automatic routing: the wire leaves the output, turns once, and
/// arrives at the input. Routing around obstacles is a research problem and a
/// schedule risk; a predictable elbow is what a person drawing a circuit
/// expects anyway.
class WireItem : public QGraphicsPathItem {
public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    WireItem(CircuitScene* scene, lb::WireId id);

    lb::WireId wireId() const { return m_id; }

    /// Recomputes the path from the two pin positions and recolours it.
    void refresh();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    CircuitScene* m_scene;
    lb::WireId m_id;
};

} // namespace lbgui
