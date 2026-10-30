#include "ComponentItem.h"

#include "CircuitDocument.h"
#include "CircuitScene.h"
#include "Theme.h"

#include "logicbench/IoComponents.h"
#include "logicbench/SubCircuit.h"

#include <QCursor>
#include <QFont>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

#include <algorithm>

namespace lbgui {

namespace {
constexpr qreal kMinimumBodyHeight = 34.0;
constexpr qreal kPinHitRadius = 7.0;
} // namespace

ComponentItem::ComponentItem(CircuitScene* scene, lb::ComponentId id)
    : m_scene(scene)
    , m_id(id)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(NoCache);
    refreshGeometry();
}

const lb::Component* ComponentItem::component() const
{
    return m_scene->document()->circuit().component(m_id);
}

void ComponentItem::refreshGeometry()
{
    const lb::Component* c = component();
    if (!c) {
        m_body = QRectF(0, 0, theme::kBodyWidth, kMinimumBodyHeight);
        return;
    }

    const std::size_t inputs = c->inputPins().size();
    const std::size_t outputs = c->outputPins().size();
    const auto rows = static_cast<qreal>(std::max<std::size_t>({inputs, outputs, 2}));

    qreal width = theme::kBodyWidth;
    const QString type = QString::fromStdString(c->typeName());
    if (type == QLatin1String("SUBCIRCUIT")) {
        // Blocks carry pin labels inside, so they need room for the longest.
        QFont font;
        font.setPointSizeF(8.0);
        const QFontMetricsF metrics(font);
        qreal widest = 0.0;
        for (std::size_t p = 0; p < c->pinCount(); ++p)
            widest = std::max(widest, metrics.horizontalAdvance(QString::fromStdString(c->pin(p).name())));
        width = std::max(theme::kBodyWidth + 20.0, widest * 2.0 + 28.0);
    }

    prepareGeometryChange();
    const qreal height = std::max(kMinimumBodyHeight, rows * theme::kPinSpacing + 8.0);
    m_body = QRectF(0, 0, width, height);
}

QRectF ComponentItem::boundingRect() const
{
    // Room for the pin stubs on both sides and for the label underneath.
    return m_body.adjusted(-theme::kPinLength - 4.0, -14.0, theme::kPinLength + 4.0, 18.0);
}

QPainterPath ComponentItem::shape() const
{
    QPainterPath path;
    path.addRect(m_body.adjusted(-theme::kPinLength, 0, theme::kPinLength, 0));
    return path;
}

QPointF ComponentItem::pinPosition(int pinIndex) const
{
    const lb::Component* c = component();
    if (!c || pinIndex < 0 || static_cast<std::size_t>(pinIndex) >= c->pinCount())
        return m_body.center();

    const bool isInput = c->pin(static_cast<std::size_t>(pinIndex)).isInput();
    const std::vector<int> side = isInput ? c->inputPins() : c->outputPins();
    const auto position = std::find(side.begin(), side.end(), pinIndex);
    const auto index = static_cast<qreal>(std::distance(side.begin(), position));
    const auto count = static_cast<qreal>(side.size());

    const qreal spacing = m_body.height() / (count + 1.0);
    const qreal y = spacing * (index + 1.0);
    const qreal x = isInput ? -theme::kPinLength : m_body.width() + theme::kPinLength;
    return QPointF(x, y);
}

int ComponentItem::pinAt(QPointF localPosition) const
{
    const lb::Component* c = component();
    if (!c)
        return -1;
    for (std::size_t p = 0; p < c->pinCount(); ++p) {
        const QPointF centre = pinPosition(static_cast<int>(p));
        if (QLineF(centre, localPosition).length() <= kPinHitRadius)
            return static_cast<int>(p);
    }
    return -1;
}

QVariant ComponentItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange && scene() && m_scene->snapToGrid()) {
        // Snap to the grid so wires stay on clean right angles.
        QPointF wanted = value.toPointF();
        wanted.setX(qRound(wanted.x() / theme::kGrid) * theme::kGrid);
        wanted.setY(qRound(wanted.y() / theme::kGrid) * theme::kGrid);
        return wanted;
    }
    if (change == ItemPositionHasChanged)
        m_scene->componentItemMoved(this);
    return QGraphicsObject::itemChange(change, value);
}

void ComponentItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    const int pin = pinAt(event->pos());
    if (pin >= 0 && event->button() == Qt::LeftButton) {
        // Starting a wire, not moving the component.
        m_pressedOnBody = false;
        m_scene->beginWire(this, pin);
        event->accept();
        return;
    }

    m_pressedOnBody = true;
    m_pressScenePos = event->scenePos();
    // Selection is updated by the base class, and the move needs to know what
    // ended up selected — so record the origins after it, not before.
    QGraphicsObject::mousePressEvent(event);
    m_scene->beginMove();
}

void ComponentItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    if (!m_pressedOnBody)
        return;
    m_pressedOnBody = false;

    const qreal travelled = QLineF(m_pressScenePos, event->scenePos()).length();
    const lb::Component* c = component();
    if (travelled < 3.0 && c && c->typeName() == "SWITCH") {
        // A click that did not drag is a poke: flip the switch.
        m_scene->document()->toggleSwitch(m_id);
        return;
    }
    m_scene->endMove();
}

void ComponentItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    const int pin = pinAt(event->pos());
    if (pin != m_hoverPin) {
        m_hoverPin = pin;
        setCursor(pin >= 0 ? Qt::CrossCursor : Qt::ArrowCursor);
        update();
    }
    QGraphicsObject::hoverMoveEvent(event);
}

void ComponentItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_hoverPin = -1;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

// ----------------------------------------------------------- painting ------

void ComponentItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const lb::Component* c = component();
    if (!c)
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    const QString type = QString::fromStdString(c->typeName());
    if (type == QLatin1String("SWITCH"))
        paintSwitch(painter);
    else if (type == QLatin1String("LED"))
        paintLed(painter);
    else if (type == QLatin1String("CLOCK"))
        paintClock(painter);
    else if (type == QLatin1String("DFF"))
        paintFlipFlop(painter);
    else if (type == QLatin1String("SUBCIRCUIT"))
        paintBlock(painter);
    else
        paintGate(painter, type);

    paintPins(painter);

    // Selection and the oscillation highlight go on last so they are never
    // hidden by the body.
    if (m_scene->document()->isOscillating(m_id)) {
        QPen pen(theme::oscillationColor(), 2.0, Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_body.adjusted(-4, -4, 4, 4));
    }
    if (isSelected()) {
        QPen pen(theme::selectionColor(), 1.5, Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_body.adjusted(-3, -3, 3, 3));
    }

    // The user's label sits under the body.
    const QString label = QString::fromStdString(c->label());
    if (!label.isEmpty()) {
        QFont font = painter->font();
        font.setPointSizeF(8.5);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(theme::labelColor());
        painter->drawText(QRectF(m_body.left() - 20, m_body.bottom() + 1,
                                 m_body.width() + 40, 14),
                          Qt::AlignHCenter | Qt::AlignTop, label);
    }
}

void ComponentItem::paintGate(QPainter* painter, const QString& type)
{
    painter->setPen(QPen(theme::outlineColor(), 1.6));
    painter->setBrush(theme::bodyColor());

    const QRectF body = m_body;
    const bool inverted = (type == QLatin1String("NAND") || type == QLatin1String("NOR")
                           || type == QLatin1String("NOT"));
    const qreal bubble = inverted ? 5.0 : 0.0;
    const QRectF shapeRect = body.adjusted(0, 0, -bubble * 2.0, 0);

    QPainterPath path;
    if (type == QLatin1String("AND") || type == QLatin1String("NAND")) {
        // Flat back, semicircular nose.
        const qreal radius = shapeRect.height() / 2.0;
        path.moveTo(shapeRect.left(), shapeRect.top());
        path.lineTo(shapeRect.right() - radius, shapeRect.top());
        path.arcTo(QRectF(shapeRect.right() - radius * 2.0, shapeRect.top(),
                          radius * 2.0, radius * 2.0), 90, -180);
        path.lineTo(shapeRect.left(), shapeRect.bottom());
        path.closeSubpath();
    } else if (type == QLatin1String("NOT")) {
        path.moveTo(shapeRect.topLeft());
        path.lineTo(shapeRect.right(), shapeRect.center().y());
        path.lineTo(shapeRect.bottomLeft());
        path.closeSubpath();
    } else {
        // OR family: concave back, pointed nose.
        const qreal inset = shapeRect.width() * 0.25;
        path.moveTo(shapeRect.left(), shapeRect.top());
        path.cubicTo(shapeRect.left() + inset, shapeRect.top() + shapeRect.height() * 0.15,
                     shapeRect.left() + inset, shapeRect.bottom() - shapeRect.height() * 0.15,
                     shapeRect.left(), shapeRect.bottom());
        path.cubicTo(shapeRect.center().x(), shapeRect.bottom(),
                     shapeRect.right() - inset * 0.2, shapeRect.center().y() + shapeRect.height() * 0.28,
                     shapeRect.right(), shapeRect.center().y());
        path.cubicTo(shapeRect.right() - inset * 0.2, shapeRect.center().y() - shapeRect.height() * 0.28,
                     shapeRect.center().x(), shapeRect.top(),
                     shapeRect.left(), shapeRect.top());
    }
    painter->drawPath(path);

    if (type == QLatin1String("XOR")) {
        // The second back line that marks exclusive-or.
        QPainterPath extra;
        const qreal inset = shapeRect.width() * 0.25;
        extra.moveTo(shapeRect.left() - 5.0, shapeRect.top());
        extra.cubicTo(shapeRect.left() + inset - 5.0, shapeRect.top() + shapeRect.height() * 0.15,
                      shapeRect.left() + inset - 5.0, shapeRect.bottom() - shapeRect.height() * 0.15,
                      shapeRect.left() - 5.0, shapeRect.bottom());
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(extra);
    }

    if (inverted) {
        painter->setBrush(theme::bodyColor());
        painter->drawEllipse(QPointF(shapeRect.right() + bubble, shapeRect.center().y()),
                             bubble, bubble);
    }
}

void ComponentItem::paintSwitch(QPainter* painter)
{
    const lb::Component* c = component();
    const auto* toggle = static_cast<const lb::Switch*>(c);
    const lb::Signal state = toggle->state();

    painter->setPen(QPen(theme::outlineColor(), 1.6));
    painter->setBrush(theme::bodyColor());
    painter->drawRoundedRect(m_body, 4, 4);

    // A lozenge that slides to the side matching the state, so the position
    // is readable across the canvas without reading the digit, and the digit
    // beside it for when it is not.
    const QRectF track = m_body.adjusted(6, 8, -22, -8);
    painter->setBrush(QColor(0xE8, 0xEC, 0xF1));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);

    const qreal knob = track.height();
    const qreal x = (state == lb::Signal::High) ? track.right() - knob : track.left();
    painter->setBrush(theme::signalColor(state));
    painter->drawEllipse(QRectF(x, track.top(), knob, knob));

    painter->setPen(theme::labelColor());
    QFont font = painter->font();
    font.setPointSizeF(10.0);
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(QRectF(m_body.right() - 20, m_body.top(), 16, m_body.height()),
                      Qt::AlignCenter, QString(QChar(lb::toChar(state))));
}

void ComponentItem::paintLed(QPainter* painter)
{
    const lb::Signal value = component()->pin(0).signal();
    const QRectF circle(m_body.center().x() - 13, m_body.center().y() - 13, 26, 26);

    if (value == lb::Signal::High) {
        // A soft halo so a lit lamp reads at a glance across the canvas.
        QRadialGradient glow(circle.center(), 22);
        glow.setColorAt(0.0, QColor(0x22, 0xA6, 0x55, 150));
        glow.setColorAt(1.0, QColor(0x22, 0xA6, 0x55, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(circle.adjusted(-9, -9, 9, 9));
    }

    painter->setPen(QPen(theme::outlineColor(), 1.6));
    painter->setBrush(theme::signalColor(value));
    painter->drawEllipse(circle);

    painter->setPen(QColor(0xFF, 0xFF, 0xFF));
    QFont font = painter->font();
    font.setPointSizeF(9.0);
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(circle, Qt::AlignCenter, QString(QChar(lb::toChar(value))));
}

void ComponentItem::paintClock(QPainter* painter)
{
    painter->setPen(QPen(theme::outlineColor(), 1.6));
    painter->setBrush(theme::bodyColor());
    painter->drawRoundedRect(m_body, 4, 4);

    // A little square wave, drawn in the colour of the current output.
    const QRectF inner = m_body.adjusted(8, 10, -8, -10);
    QPainterPath wave;
    const qreal step = inner.width() / 4.0;
    wave.moveTo(inner.left(), inner.bottom());
    wave.lineTo(inner.left(), inner.top());
    wave.lineTo(inner.left() + step, inner.top());
    wave.lineTo(inner.left() + step, inner.bottom());
    wave.lineTo(inner.left() + step * 2, inner.bottom());
    wave.lineTo(inner.left() + step * 2, inner.top());
    wave.lineTo(inner.left() + step * 3, inner.top());
    wave.lineTo(inner.left() + step * 3, inner.bottom());
    wave.lineTo(inner.right(), inner.bottom());

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(theme::signalColor(component()->pin(0).signal()), 1.8));
    painter->drawPath(wave);
}

void ComponentItem::paintFlipFlop(QPainter* painter)
{
    painter->setPen(QPen(theme::outlineColor(), 1.6));
    painter->setBrush(theme::bodyColor());
    painter->drawRect(m_body);

    QFont font = painter->font();
    font.setPointSizeF(7.5);
    painter->setFont(font);
    painter->setPen(theme::labelColor());

    const lb::Component* c = component();
    for (std::size_t p = 0; p < c->pinCount(); ++p) {
        const QPointF pin = pinPosition(static_cast<int>(p));
        const bool isInput = c->pin(p).isInput();
        const QRectF slot(isInput ? m_body.left() + 3 : m_body.right() - 27,
                          pin.y() - 7, 24, 14);
        painter->drawText(slot, isInput ? Qt::AlignLeft | Qt::AlignVCenter
                                        : Qt::AlignRight | Qt::AlignVCenter,
                          QString::fromStdString(c->pin(p).name()));
    }

    // The wedge that marks an edge-triggered clock input.
    const int clockPin = c->pinIndex("CLK");
    if (clockPin >= 0) {
        const QPointF pin = pinPosition(clockPin);
        QPainterPath wedge;
        wedge.moveTo(m_body.left(), pin.y() - 5);
        wedge.lineTo(m_body.left() + 7, pin.y());
        wedge.lineTo(m_body.left(), pin.y() + 5);
        painter->setPen(QPen(theme::outlineColor(), 1.4));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(wedge);
    }
}

void ComponentItem::paintBlock(QPainter* painter)
{
    const auto* block = static_cast<const lb::SubCircuit*>(component());

    painter->setPen(QPen(theme::outlineColor(), 1.8));
    painter->setBrush(QColor(0xEF, 0xF4, 0xFF));
    painter->drawRoundedRect(m_body, 3, 3);

    QFont font = painter->font();
    font.setPointSizeF(8.0);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(theme::labelColor());
    painter->drawText(QRectF(m_body.left(), m_body.top() + 2, m_body.width(), 14),
                      Qt::AlignCenter, QString::fromStdString(block->blockName()));

    font.setBold(false);
    font.setPointSizeF(7.5);
    painter->setFont(font);
    for (std::size_t p = 0; p < block->pinCount(); ++p) {
        const QPointF pin = pinPosition(static_cast<int>(p));
        const bool isInput = block->pin(p).isInput();
        const QRectF slot(isInput ? m_body.left() + 4 : m_body.center().x(),
                          pin.y() - 7, m_body.width() / 2.0 - 4, 14);
        painter->drawText(slot, isInput ? Qt::AlignLeft | Qt::AlignVCenter
                                        : Qt::AlignRight | Qt::AlignVCenter,
                          QString::fromStdString(block->pin(p).name()));
    }
}

void ComponentItem::paintPins(QPainter* painter)
{
    const lb::Component* c = component();
    for (std::size_t p = 0; p < c->pinCount(); ++p) {
        const QPointF pin = pinPosition(static_cast<int>(p));
        const bool isInput = c->pin(p).isInput();
        const QPointF anchor(isInput ? m_body.left() : m_body.right(), pin.y());

        painter->setPen(QPen(theme::signalColor(c->pin(p).signal()), 1.8));
        painter->drawLine(anchor, pin);

        const bool hovered = (static_cast<int>(p) == m_hoverPin);
        painter->setBrush(hovered ? theme::selectionColor() : theme::signalColor(c->pin(p).signal()));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(pin, hovered ? 4.0 : 2.6, hovered ? 4.0 : 2.6);
    }
}

} // namespace lbgui
