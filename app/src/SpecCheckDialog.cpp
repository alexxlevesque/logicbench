#include "SpecCheckDialog.h"

#include "CircuitDocument.h"

#include "logicbench/SpecChecker.h"

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

#include <algorithm>

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

    connect(m_onlyDiffering, &QCheckBox::toggled, this, &SpecCheckDialog::recheck);
    connect(m_path, &QLineEdit::returnPressed, this, &SpecCheckDialog::recheck);

    m_verdict->hide();
    m_matched->setText(tr("Choose the spec file (Browse…) to compare this circuit with it."));
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

int SpecCheckDialog::rowCount() const
{
    return m_rows->rowCount();
}

void SpecCheckDialog::showVerdict(bool passed, const QString& text)
{
    m_verdict->setStyleSheet(passed
        ? QStringLiteral("QLabel { color:#3C7A50; font-weight:600; background:#EEF6F0;"
                         " border:1px solid #3C7A50; padding:8px 10px; }")
        : QStringLiteral("QLabel { color:#B3261E; font-weight:600; background:#FBEDEC;"
                         " border:1px solid #B3261E; padding:8px 10px; }"));
    m_verdict->setText(text);
    m_verdict->show();
}

void SpecCheckDialog::recheck()
{
    m_rows->clear();
    m_rows->setRowCount(0);
    m_rows->setColumnCount(0);
    const QString path = m_path->text().trimmed();
    if (path.isEmpty())
        return;

    lb::Specification spec;
    try {
        spec = lb::Specification::load(path.toStdString());
    } catch (const lb::SpecFormatError& e) {
        // The file is the problem, not the circuit: say which line.
        m_matched->setText(QString());
        showVerdict(false, e.line() > 0 ? tr("✕  The spec cannot be read: line %1, %2")
                                              .arg(e.line()).arg(QString::fromStdString(e.message()))
                                        : tr("✕  %1").arg(QString::fromStdString(e.message())));
        return;
    }

    // The spec is never modified, and the circuit is left as it was found.
    const lb::SpecCheckResult result = lb::SpecChecker::check(m_document->circuit(), spec);
    m_lastResult = result;
    const auto join = [](const std::vector<std::string>& names) {
        QStringList list;
        for (const std::string& n : names)
            list << QString::fromStdString(n);
        return list.join(QLatin1Char(' '));
    };
    m_matched->setText(result.inputColumns.empty() && result.outputColumns.empty()
                           ? QString()
                           : tr("Matched by column name. Inputs: %1 · Outputs: %2")
                                 .arg(join(result.inputColumns), join(result.outputColumns)));

    switch (result.status) {
    case lb::SpecCheckStatus::Pass:
        showVerdict(true, tr("✓  All %1 rows match the spec").arg(result.rowsCompared));
        break;
    case lb::SpecCheckStatus::Fail: {
        QStringList outputs;
        for (const lb::Mismatch& m : result.mismatches)
            if (!outputs.contains(QString::fromStdString(m.output)))
                outputs << QString::fromStdString(m.output);
        showVerdict(false, tr("✕  %1 of %2 rows differ, on output %3")
                               .arg(result.rowsDiffering).arg(result.rowsCompared)
                               .arg(outputs.join(QStringLiteral(", "))));
        break;
    }
    default:
        showVerdict(false, QStringLiteral("✕  ") + QString::fromStdString(result.message));
        return;
    }
    fillRows(result, spec);
}

void SpecCheckDialog::fillRows(const lb::SpecCheckResult& result, const lb::Specification& spec)
{
    QStringList headers;
    for (const std::string& name : result.inputColumns)
        headers << QString::fromStdString(name);
    headers << tr("Output") << tr("Specification") << tr("Your circuit");

    // Every row of the spec, or only the differing ones; a differing output
    // gets its own row so each mismatch is one line to read.
    struct Line { QStringList cells; bool differs; };
    QVector<Line> lines;
    std::size_t next = 0;
    for (std::size_t r = 0; r < spec.rowCount(); ++r) {
        QStringList inputs;
        for (const std::string& name : result.inputColumns) {
            const auto& cols = spec.columns();
            const auto col = static_cast<std::size_t>(std::find(cols.begin(), cols.end(), name) - cols.begin());
            inputs << QString(QChar(lb::toChar(spec.rows()[r][col])));
        }
        bool any = false;
        while (next < result.mismatches.size() && result.mismatches[next].specRow == r) {
            const lb::Mismatch& m = result.mismatches[next++];
            lines.append({inputs + QStringList{QString::fromStdString(m.output),
                                               QString(QChar(lb::toChar(m.expected))),
                                               QString(QChar(lb::toChar(m.actual)))},
                          true});
            any = true;
        }
        if (!any && !m_onlyDiffering->isChecked())
            lines.append({inputs + QStringList{tr("all"), tr("match"), tr("match")}, false});
    }

    m_rows->setColumnCount(static_cast<int>(headers.size()));
    m_rows->setHorizontalHeaderLabels(headers);
    m_rows->setRowCount(static_cast<int>(lines.size()));
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const int inputCount = static_cast<int>(result.inputColumns.size());
    for (int r = 0; r < lines.size(); ++r) {
        for (int c = 0; c < headers.size(); ++c) {
            auto* cell = new QTableWidgetItem(lines[r].cells[c]);
            cell->setTextAlignment(Qt::AlignCenter);
            if (lines[r].differs)
                cell->setBackground(QColor(0xFB, 0xF1, 0xF0));
            if (c != inputCount)
                cell->setFont(mono);
            if (lines[r].differs && c == static_cast<int>(headers.size()) - 1) {
                QFont bold = mono;
                bold.setBold(true);
                cell->setFont(bold);
                cell->setForeground(QColor(0xB3, 0x26, 0x1E));
            }
            m_rows->setItem(r, c, cell);
        }
    }
    m_rows->resizeColumnsToContents();
    for (int c = 0; c < inputCount; ++c)
        m_rows->setColumnWidth(c, 52);
}

} // namespace lbgui
