#pragma once

#include <QImage>
#include <QRectF>
#include <QString>

namespace lbgui {

class CircuitScene;
class WaveformWidget;

/// The choices on the Export image window.
struct ExportOptions {
    enum class Format { Png, Svg };

    Format format = Format::Png;
    int scale = 2;                  ///< 1 or 2; PNG only
    bool selectionOnly = false;
    bool transparent = false;       ///< no white background
    bool signalColours = true;      ///< 0/1/X colours and values, as on screen
    bool labels = true;
    qreal margin = 16.0;            ///< canvas units around the parts
};

/// Writes the canvas, or part of it, as PNG or SVG, and the timing diagram as
/// PNG (FR-8).
///
/// The export is drawn by the scene itself, so it looks like the screen: the
/// same symbols, the same wire colours. Only the grid and the selection
/// highlight are left out, plus whatever the options turn off.
class ImageExporter {
public:
    /// The part of the scene an export covers: every component, or only the
    /// selected ones, with the margin. Empty if there is nothing to draw.
    static QRectF contentBounds(CircuitScene& scene, bool selectionOnly, qreal margin);

    /// Renders to an image at the given scale (PNG path). `area` empty means
    /// contentBounds().
    static QImage renderScene(CircuitScene& scene, const ExportOptions& options,
                              QRectF area = QRectF());

    /// Writes the file. Returns false and fills `error` if there is nothing to
    /// export or the file cannot be written.
    static bool exportScene(CircuitScene& scene, const ExportOptions& options, const QString& path,
                            QString* error, QRectF area = QRectF());

    /// The timing diagram as drawn, at 1x or 2x.
    static bool exportWaveform(WaveformWidget& widget, int scale, const QString& path, QString* error);
};

} // namespace lbgui
