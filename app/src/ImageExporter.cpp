#include "ImageExporter.h"

#include "CircuitScene.h"
#include "ComponentItem.h"
#include "WaveformDock.h"

#include <QGraphicsItem>
#include <QPainter>
#include <QSvgGenerator>

#include <functional>

namespace lbgui {

namespace {

/// Sets the scene up for an export and puts everything back afterwards, even
/// if the render throws: style, selection, and the items hidden for a
/// selection-only export.
class ExportStaging {
public:
    ExportStaging(CircuitScene& scene, const ExportOptions& options)
        : m_scene(scene)
        , m_style(scene.renderStyle())
        , m_selected(scene.selectedItems())
    {
        CircuitScene::RenderStyle style;
        style.grid = false;
        style.background = !options.transparent;
        style.signalColours = options.signalColours;
        style.labels = options.labels;
        scene.setRenderStyle(style);

        if (options.selectionOnly) {
            for (QGraphicsItem* item : scene.items()) {
                if (item->isVisible() && !item->isSelected() && item->parentItem() == nullptr) {
                    // A wire between two selected parts stays.
                    if (item->type() != ComponentItem::Type && wireInside(item))
                        continue;
                    item->setVisible(false);
                    m_hidden.append(item);
                }
            }
        }
        for (QGraphicsItem* item : m_selected)
            item->setSelected(false);         // no dashed highlight in the picture
    }

    ~ExportStaging()
    {
        for (QGraphicsItem* item : m_hidden)
            item->setVisible(true);
        for (QGraphicsItem* item : m_selected)
            item->setSelected(true);
        m_scene.setRenderStyle(m_style);
    }

private:
    bool wireInside(QGraphicsItem* wire) const
    {
        // A wire is inside the selection when both of its ends touch selected
        // components; test the ends of its outline against them.
        const QRectF bounds = wire->sceneBoundingRect();
        int touching = 0;
        for (QGraphicsItem* item : m_selected) {
            if (item->type() == ComponentItem::Type && item->sceneBoundingRect().intersects(bounds))
                ++touching;
        }
        return touching >= 2;
    }

    CircuitScene& m_scene;
    CircuitScene::RenderStyle m_style;
    QList<QGraphicsItem*> m_selected;
    QList<QGraphicsItem*> m_hidden;
};

} // namespace

QRectF ImageExporter::contentBounds(CircuitScene& scene, bool selectionOnly, qreal margin)
{
    QRectF bounds;
    for (QGraphicsItem* item : scene.items()) {
        if (item->type() != ComponentItem::Type)
            continue;
        if (selectionOnly && !item->isSelected())
            continue;
        bounds = bounds.united(item->sceneBoundingRect());
    }
    if (bounds.isNull())
        return QRectF();
    if (!selectionOnly) {
        // Wires can bend outside the parts they join.
        for (QGraphicsItem* item : scene.items())
            if (item->type() != ComponentItem::Type && item->parentItem() == nullptr)
                bounds = bounds.united(item->sceneBoundingRect());
    }
    return bounds.adjusted(-margin, -margin, margin, margin);
}

QImage ImageExporter::renderScene(CircuitScene& scene, const ExportOptions& options, QRectF area)
{
    if (area.isEmpty())
        area = contentBounds(scene, options.selectionOnly, options.margin);
    if (area.isEmpty())
        return QImage();

    const int scale = options.scale >= 2 ? 2 : 1;
    QImage image(area.size().toSize() * scale, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        ExportStaging staging(scene, options);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        scene.render(&painter, QRectF(QPointF(0, 0), QSizeF(image.size())), area, Qt::IgnoreAspectRatio);
    }
    return image;
}

bool ImageExporter::exportScene(CircuitScene& scene, const ExportOptions& options, const QString& path,
                                QString* error, QRectF area)
{
    if (area.isEmpty())
        area = contentBounds(scene, options.selectionOnly, options.margin);
    if (area.isEmpty()) {
        if (error)
            *error = options.selectionOnly ? QObject::tr("Select the components to export first.")
                                           : QObject::tr("The circuit is empty: there is nothing to export.");
        return false;
    }

    if (options.format == ExportOptions::Format::Png) {
        const QImage image = renderScene(scene, options, area);
        if (image.isNull() || !image.save(path, "PNG")) {
            if (error)
                *error = QObject::tr("Cannot write %1.").arg(path);
            return false;
        }
        return true;
    }

    QSvgGenerator svg;
    svg.setFileName(path);
    svg.setSize(area.size().toSize());
    svg.setViewBox(QRectF(QPointF(0, 0), area.size()));
    svg.setTitle(QStringLiteral("LogicBench circuit"));
    {
        ExportStaging staging(scene, options);
        QPainter painter;
        if (!painter.begin(&svg)) {
            if (error)
                *error = QObject::tr("Cannot write %1.").arg(path);
            return false;
        }
        scene.render(&painter, QRectF(QPointF(0, 0), area.size()), area, Qt::IgnoreAspectRatio);
        painter.end();
    }
    return true;
}

bool ImageExporter::exportWaveform(WaveformWidget& widget, int scale, const QString& path, QString* error)
{
    scale = scale >= 2 ? 2 : 1;
    QImage image(widget.size() * scale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(scale);
    image.fill(Qt::white);
    widget.render(&image);
    if (!image.save(path, "PNG")) {
        if (error)
            *error = QObject::tr("Cannot write %1.").arg(path);
        return false;
    }
    return true;
}

} // namespace lbgui
