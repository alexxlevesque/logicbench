#include "SpecCheckDialog.h"

#include "CircuitDocument.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace lbgui {

namespace {

QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color:#7A828C;"));
    return label;
}

} // namespace

SpecCheckDialog::SpecCheckDialog(CircuitDocument* document, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_path(new QLineEdit(this))
    , m_matched(hintLabel(QString(), this))
    , m_verdict(new QLabel(this))
    , m_onlyDiffering(new QCheckBox(tr("Show only rows that differ"), this))
    , m_rows(new QTableWidget(this))
{
    setWindowTitle(tr("Check against specification — %1").arg(m_document->displayName()));
    resize(640, 420);

    auto* browseButton = new QPushButton(tr("Browse…"), this);
    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(new QLabel(tr("Specification"), this));
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browseButton);

    m_verdict->setStyleSheet(QStringLiteral(
        "QLabel { color:#B3261E; font-weight:600; background:#FBEDEC;"
        " border:1px solid #B3261E; padding:8px 10px; }"));
    m_onlyDiffering->setChecked(true);

    m_rows->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rows->setSelectionMode(QAbstractItemView::NoSelection);
    m_rows->verticalHeader()->setVisible(false);
    m_rows->horizontalHeader()->setStretchLastSection(true);

    auto* buttons = new QDialogButtonBox(this);
    auto* recheckButton = buttons->addButton(tr("Re-check"), QDialogButtonBox::ActionRole);
    auto* closeButton = buttons->addButton(QDialogButtonBox::Close);
    recheckButton->setAutoDefault(false);
    browseButton->setAutoDefault(false);
    closeButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(pathRow);
    layout->addWidget(m_matched);
    layout->addSpacing(4);
    layout->addWidget(m_verdict);
    layout->addWidget(m_onlyDiffering);
    layout->addWidget(m_rows, 1);
    layout->addWidget(hintLabel(tr("A spec is an exported truth table (Tools › Truth table "
                                   "› Export CSV) from a reference circuit."),
                                this));
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, &SpecCheckDialog::browse);
    connect(recheckButton, &QPushButton::clicked, this, &SpecCheckDialog::recheck);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setSpecPath(QStringLiteral("lab3_comparator_spec.csv"));
}

void SpecCheckDialog::setSpecPath(const QString& path)
{
    m_path->setText(path);
    recheck();
}

void SpecCheckDialog::browse()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open specification"), QString(),
                                                      tr("CSV files (*.csv)"));
    if (!path.isEmpty())
        setSpecPath(path);
}

void SpecCheckDialog::recheck()
{
    // Not wired to lb::SpecChecker yet (FR-4); always shows the scenario.
    showSampleResult();
}

void SpecCheckDialog::showSampleResult()
{
    m_matched->setText(tr("Matched by column name. Inputs: A1 A0 B1 B0 · Outputs: GT EQ LT"));
    m_verdict->setText(tr("✕  2 of 16 rows differ, all on output GT"));

    const QStringList headers{tr("A1"), tr("A0"), tr("B1"), tr("B0"), tr("Output"),
                              tr("Specification"), tr("Your circuit")};
    const QList<QStringList> rows{
        {QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("0"),
         QStringLiteral("GT"), QStringLiteral("1"), QStringLiteral("0")},
        {QStringLiteral("1"), QStringLiteral("1"), QStringLiteral("1"), QStringLiteral("0"),
         QStringLiteral("GT"), QStringLiteral("1"), QStringLiteral("0")},
    };

    m_rows->clear();
    m_rows->setColumnCount(static_cast<int>(headers.size()));
    m_rows->setHorizontalHeaderLabels(headers);
    m_rows->setRowCount(static_cast<int>(rows.size()));
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < headers.size(); ++c) {
            auto* cell = new QTableWidgetItem(rows[r][c]);
            cell->setTextAlignment(Qt::AlignCenter);
            cell->setBackground(QColor(0xFB, 0xF1, 0xF0));
            if (c != 4)
                cell->setFont(mono);
            if (c == static_cast<int>(headers.size()) - 1) {
                QFont bold = mono;
                bold.setBold(true);
                cell->setFont(bold);
                cell->setForeground(QColor(0xB3, 0x26, 0x1E));
            }
            m_rows->setItem(r, c, cell);
        }
    }
    m_rows->resizeColumnsToContents();
    for (int c = 0; c < 4; ++c)
        m_rows->setColumnWidth(c, 52);
}

} // namespace lbgui
