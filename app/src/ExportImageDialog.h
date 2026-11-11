#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QRadioButton;

namespace lbgui {

/// Export image (FR-8): the whole circuit, the selection or the timing
/// diagram, as PNG or SVG.
///
/// Shell for the SDD screen capture. When wired in, Export… asks for a file
/// name and hands these choices to ImageExporter.
class ExportImageDialog : public QDialog {
    Q_OBJECT
public:
    enum class Target { WholeCircuit, Selection, TimingDiagram };
    enum class Format { Png, Svg };

    explicit ExportImageDialog(bool hasSelection, QWidget* parent = nullptr);

    Target target() const;
    Format format() const;
    /// 1 or 2; ignored for SVG.
    int scale() const;
    bool showSignals() const;
    bool transparentBackground() const;
    bool includeLabels() const;

private slots:
    void updateEnabled();

private:
    QRadioButton* m_whole;
    QRadioButton* m_selection;
    QRadioButton* m_timing;
    QComboBox* m_format;
    QComboBox* m_scale;
    QCheckBox* m_signals;
    QCheckBox* m_transparent;
    QCheckBox* m_labels;
};

} // namespace lbgui
