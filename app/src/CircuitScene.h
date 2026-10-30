#pragma once

#include "logicbench/Pin.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QPointF>

#include <memory>

namespace lbgui {

class CircuitDocument;
class ComponentItem;
class WireItem;

/// The canvas. Mirrors the document's circuit as graphics items and turns
/// mouse gestures into undoable commands — it never edits the circuit itself.
class CircuitScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit CircuitScene(CircuitDocument* document, QObject* parent = nullptr);

    CircuitDocument* document() const { return m_document; }
    ComponentItem* itemFor(lb::ComponentId id) const { return m_components.value(id, nullptr); }

    /// Rebuilds every item from the circuit. Used after a structural change.
    void rebuild();
    /// Repaints without rebuilding: signal values changed, shapes did not.
    void refreshSignals();
    /// Re-reads positions from the model and re-routes the wires.
    void syncGeometry();

    /// Called by ComponentItem when a wire drag starts on one of its pins.
    void beginWire(ComponentItem* item, int pin);
    void beginMove();
    void endMove();
    void componentItemMoved(ComponentItem* item);

    QList<lb::ComponentId> selectedComponentIds() const;
    void deleteSelection();

    /// The MIME type the palette uses when a component is dragged onto the
    /// canvas.
    static const char* componentMimeType() { return "application/x-logicbench-type"; }

    /// Builds a component from a palette payload: either a registered type
    /// name, or "@block:<path>" for a circuit file to wrap as a block.
    /// Throws whatever the factory or the serializer throws.
    static std::unique_ptr<lb::Component> createFromPayload(const QString& payload);

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }

signals:
    void selectionDescriptionChanged(const QString& text);
    void statusMessage(const QString& text);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
    void cancelWire();
    void finishWire(QPointF scenePosition);
    void refreshWiresFor(lb::ComponentId id);

    CircuitDocument* m_document;
    QHash<lb::ComponentId, ComponentItem*> m_components;
    QHash<lb::WireId, WireItem*> m_wires;

    // wire being dragged
    ComponentItem* m_wireSource = nullptr;
    int m_wireSourcePin = -1;
    QGraphicsPathItem* m_wirePreview = nullptr;

    // drag of a selection
    QHash<lb::ComponentId, QPointF> m_moveOrigins;
    bool m_rebuilding = false;
    bool m_snapToGrid = true;
};

/// A view with wheel zoom and middle-button panning. Everything else about
/// the canvas lives in the scene.
class CircuitView : public QGraphicsView {
    Q_OBJECT

public:
    explicit CircuitView(CircuitScene* scene, QWidget* parent = nullptr);

    void zoomIn()  { applyZoom(1.15); }
    void zoomOut() { applyZoom(1.0 / 1.15); }
    void resetZoom();

    /// Centres the view on the circuit, shrinking to fit if it does not
    /// already. Opening a file lands on the circuit rather than on empty
    /// canvas beside it.
    void frameCircuit();

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    void applyZoom(qreal factor);

    qreal m_zoom = 1.0;
};

} // namespace lbgui
