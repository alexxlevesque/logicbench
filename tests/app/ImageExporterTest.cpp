#include <gtest/gtest.h>

#include "../CircuitFixtures.h"

#include "CircuitDocument.h"
#include "CircuitScene.h"
#include "ComponentItem.h"
#include "ImageExporter.h"
#include "WaveformDock.h"

#include "logicbench/ComponentFactory.h"

#include <QDir>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>

using namespace lbgui;

namespace {

/// A document holding a half adder, and its scene with items built.
struct Canvas {
    CircuitDocument document;
    CircuitScene scene{&document};
    lbtest::HalfAdder parts;

    Canvas()
    {
        parts = lbtest::buildHalfAdder(document.circuit());
        int i = 0;
        for (const auto& c : document.circuit().components())
            c->setPosition((i % 2) * 140.0, (i / 2) * 80.0), ++i;
        document.resimulate();
        scene.rebuild();
    }
};

/// What the scene draws for this area under export conditions, rendered by
/// hand, to compare the exporter against.
QImage referenceRender(CircuitScene& scene, QRectF area, int scale)
{
    CircuitScene::RenderStyle style;
    style.grid = false;
    const CircuitScene::RenderStyle before = scene.renderStyle();
    scene.setRenderStyle(style);
    QImage image(area.size().toSize() * scale, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    scene.render(&painter, QRectF(QPointF(0, 0), QSizeF(image.size())), area, Qt::IgnoreAspectRatio);
    painter.end();
    scene.setRenderStyle(before);
    return image;
}

int nonWhitePixels(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) != QColor(Qt::white))
                ++count;
    return count;
}

} // namespace

TEST(ImageExporterTest, PngMatchesTheSceneAtOneAndTwoTimes)
{
    Canvas canvas;
    const QRectF area = ImageExporter::contentBounds(canvas.scene, false, 16.0);
    ASSERT_FALSE(area.isEmpty());
    for (int scale : {1, 2}) {
        ExportOptions options;
        options.scale = scale;
        const QImage exported = ImageExporter::renderScene(canvas.scene, options);
        EXPECT_EQ(exported.size(), area.size().toSize() * scale);
        EXPECT_EQ(exported, referenceRender(canvas.scene, area, scale)) << "scale " << scale;
    }
}

TEST(ImageExporterTest, ExportingLeavesTheCanvasAsItWas)
{
    Canvas canvas;
    canvas.scene.itemFor(canvas.parts.a)->setSelected(true);
    ImageExporter::renderScene(canvas.scene, ExportOptions{});
    EXPECT_TRUE(canvas.scene.itemFor(canvas.parts.a)->isSelected());
    EXPECT_TRUE(canvas.scene.renderStyle().grid);
    for (QGraphicsItem* item : canvas.scene.items())
        EXPECT_TRUE(item->isVisible());
}

TEST(ImageExporterTest, PngAndSvgFilesAreWrittenAndTheSvgParses)
{
    Canvas canvas;
    QTemporaryDir dir;
    QString error;
    const QString png = QDir(dir.path()).filePath(QStringLiteral("adder.png"));
    ASSERT_TRUE(ImageExporter::exportScene(canvas.scene, ExportOptions{}, png, &error)) << error.toStdString();
    EXPECT_FALSE(QImage(png).isNull());

    ExportOptions vector;
    vector.format = ExportOptions::Format::Svg;
    const QString svg = QDir(dir.path()).filePath(QStringLiteral("adder.svg"));
    ASSERT_TRUE(ImageExporter::exportScene(canvas.scene, vector, svg, &error)) << error.toStdString();
    QSvgRenderer renderer(svg);
    EXPECT_TRUE(renderer.isValid());
    EXPECT_GT(renderer.defaultSize().width(), 0);
}

TEST(ImageExporterTest, ASelectionExportContainsOnlyTheSelection)
{
    Canvas canvas;
    canvas.scene.itemFor(canvas.parts.xorGate)->setSelected(true);
    ExportOptions options;
    options.selectionOnly = true;
    options.scale = 1;

    const QRectF whole = ImageExporter::contentBounds(canvas.scene, false, 16.0);
    const QRectF selected = ImageExporter::contentBounds(canvas.scene, true, 16.0);
    const QImage image = ImageExporter::renderScene(canvas.scene, options);
    EXPECT_EQ(image.size(), selected.size().toSize());
    EXPECT_LT(image.width() * image.height(), static_cast<int>(whole.width() * whole.height()));

    // Nothing but the XOR gate is drawn: render it alone and compare.
    for (const auto& c : canvas.document.circuit().components())
        if (c->id() != canvas.parts.xorGate)
            canvas.scene.itemFor(c->id())->setVisible(false);
    for (QGraphicsItem* item : canvas.scene.items())
        if (item->type() != ComponentItem::Type)
            item->setVisible(false);
    canvas.scene.itemFor(canvas.parts.xorGate)->setSelected(false);
    EXPECT_EQ(image, referenceRender(canvas.scene, selected, 1));
}

TEST(ImageExporterTest, AnEmptyCircuitIsRefused)
{
    CircuitDocument document;
    CircuitScene scene(&document);
    QString error;
    EXPECT_FALSE(ImageExporter::exportScene(scene, ExportOptions{}, QStringLiteral("unused.png"), &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(ImageExporterTest, EveryNewSymbolDraws)
{
    for (const char* type : {"JKFF", "TFF", "MUX2", "MUX4", "DEC2", "DEC3", "HEX", "SEG7", "NOTE"}) {
        CircuitDocument document;
        document.circuit().add(lb::ComponentFactory::standard().create(type));
        CircuitScene scene(&document);
        scene.rebuild();
        ExportOptions options;
        options.scale = 1;
        const QImage image = ImageExporter::renderScene(scene, options);
        ASSERT_FALSE(image.isNull()) << type;
        EXPECT_GT(nonWhitePixels(image), 200) << type;      // a real symbol, not a blank box
    }
}

TEST(ImageExporterTest, TheTimingDiagramExportsAtTwoTimes)
{
    CircuitDocument document;
    WaveformDock dock(&document);
    dock.waveformWidget()->resize(400, 120);
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("timing.png"));
    QString error;
    ASSERT_TRUE(ImageExporter::exportWaveform(*dock.waveformWidget(), 2, path, &error));
    EXPECT_EQ(QImage(path).size(), QSize(800, 240));
}
