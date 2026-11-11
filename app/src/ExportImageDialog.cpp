#include "ExportImageDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace lbgui {

ExportImageDialog::ExportImageDialog(bool hasSelection, QWidget* parent)
    : QDialog(parent)
    , m_whole(new QRadioButton(tr("Whole circuit"), this))
    , m_selection(new QRadioButton(tr("Selection only"), this))
    , m_timing(new QRadioButton(tr("Timing diagram"), this))
    , m_format(new QComboBox(this))
    , m_scale(new QComboBox(this))
    , m_signals(new QCheckBox(tr("Show signal values (0, 1, X) and colours"), this))
    , m_transparent(new QCheckBox(tr("Transparent background"), this))
    , m_labels(new QCheckBox(tr("Include component labels"), this))
{
    setWindowTitle(tr("Export image"));

    QFont bold = font();
    bold.setBold(true);

    auto* whatHeading = new QLabel(tr("What"), this);
    whatHeading->setFont(bold);
    auto* what = new QHBoxLayout;
    what->addWidget(m_whole);
    what->addWidget(m_selection);
    what->addWidget(m_timing);
    what->addStretch(1);
    auto* group = new QButtonGroup(this);
    group->addButton(m_whole);
    group->addButton(m_selection);
    group->addButton(m_timing);
    m_whole->setChecked(true);
    m_selection->setEnabled(hasSelection);

    m_format->addItems({QStringLiteral("PNG"), QStringLiteral("SVG")});
    m_scale->addItems({QStringLiteral("1×"), QStringLiteral("2×")});
    m_scale->setCurrentIndex(1);
    m_format->setMinimumWidth(120);
    m_scale->setMinimumWidth(80);

    auto* formatHeading = new QLabel(tr("Format"), this);
    formatHeading->setFont(bold);
    auto* format = new QHBoxLayout;
    format->addWidget(formatHeading);
    format->addWidget(m_format);
    format->addSpacing(16);
    format->addWidget(new QLabel(tr("Scale"), this));
    format->addWidget(m_scale);
    format->addStretch(1);

    auto* hint = new QLabel(tr("SVG keeps lines sharp at any size; scale applies to PNG only."),
                            this);
    hint->setStyleSheet(QStringLiteral("color:#7A828C;"));

    m_signals->setChecked(true);
    m_labels->setChecked(true);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Cancel);
    auto* exportButton = buttons->addButton(tr("Export…"), QDialogButtonBox::AcceptRole);
    exportButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(whatHeading);
    layout->addLayout(what);
    layout->addSpacing(6);
    layout->addLayout(format);
    layout->addWidget(hint);
    layout->addSpacing(6);
    layout->addWidget(m_signals);
    layout->addWidget(m_transparent);
    layout->addWidget(m_labels);
    layout->addSpacing(6);
    layout->addWidget(buttons);

    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ExportImageDialog::updateEnabled);
    connect(m_timing, &QRadioButton::toggled, this, &ExportImageDialog::updateEnabled);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateEnabled();
}

ExportImageDialog::Target ExportImageDialog::target() const
{
    if (m_selection->isChecked())
        return Target::Selection;
    if (m_timing->isChecked())
        return Target::TimingDiagram;
    return Target::WholeCircuit;
}

ExportImageDialog::Format ExportImageDialog::format() const
{
    return m_format->currentIndex() == 1 ? Format::Svg : Format::Png;
}

int ExportImageDialog::scale() const
{
    return m_scale->currentIndex() + 1;
}

bool ExportImageDialog::showSignals() const { return m_signals->isChecked(); }
bool ExportImageDialog::transparentBackground() const { return m_transparent->isChecked(); }
bool ExportImageDialog::includeLabels() const { return m_labels->isChecked(); }

void ExportImageDialog::updateEnabled()
{
    // The timing diagram exports as PNG only (FR-8).
    if (m_timing->isChecked())
        m_format->setCurrentIndex(0);
    m_format->setEnabled(!m_timing->isChecked());
    m_scale->setEnabled(format() == Format::Png);
}

} // namespace lbgui
