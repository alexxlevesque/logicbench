#include "TruthTableDialog.h"

#include "CircuitDocument.h"
#include "Theme.h"

#include "logicbench/Component.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <fstream>

namespace lbgui {

namespace {

QString portLabel(const lb::Component& component)
{
    return component.label().empty()
               ? QStringLiteral("%1 %2")
                     .arg(QString::fromStdString(component.typeName()))
                     .arg(component.id())
               : QString::fromStdString(component.label());
}

} // namespace

TruthTableDialog::TruthTableDialog(CircuitDocument* document, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_inputs(new QListWidget(this))
    , m_outputs(new QListWidget(this))
    , m_table(new QTableWidget(this))
    , m_message(new QLabel(this))
    , m_exportButton(new QPushButton(tr("Export CSV…"), this))
{
    setWindowTitle(tr("Truth table — %1").arg(m_document->displayName()));
    resize(760, 520);

    auto* selectors = new QHBoxLayout;
    auto* inputBox = new QGroupBox(tr("Inputs (max %1)").arg(lb::TruthTableGenerator::kMaxInputs), this);
    auto* inputLayout = new QVBoxLayout(inputBox);
    inputLayout->addWidget(m_inputs);
    auto* outputBox = new QGroupBox(tr("Outputs"), this);
    auto* outputLayout = new QVBoxLayout(outputBox);
    outputLayout->addWidget(m_outputs);
    selectors->addWidget(inputBox);
    selectors->addWidget(outputBox);

    // The selectors are a short list; the table is what the user came for.
    m_inputs->setMaximumHeight(130);
    m_outputs->setMaximumHeight(130);

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->setVisible(false);
    m_message->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    auto* generateButton = buttons->addButton(tr("Generate"), QDialogButtonBox::ActionRole);
    buttons->addButton(m_exportButton, QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    m_exportButton->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(selectors, 0);
    layout->addWidget(m_message);
    layout->addWidget(m_table, 1);
    layout->addWidget(buttons);

    connect(generateButton, &QPushButton::clicked, this, &TruthTableDialog::generate);
    connect(m_exportButton, &QPushButton::clicked, this, &TruthTableDialog::exportCsv);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populateSelectors();
    generate();
}

void TruthTableDialog::populateSelectors()
{
    const lb::Circuit& circuit = m_document->circuit();

    for (lb::ComponentId id : lb::TruthTableGenerator::allSwitches(circuit)) {
        auto* item = new QListWidgetItem(portLabel(*circuit.component(id)), m_inputs);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, id);
    }
    for (lb::ComponentId id : lb::TruthTableGenerator::allLeds(circuit)) {
        auto* item = new QListWidgetItem(portLabel(*circuit.component(id)), m_outputs);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, id);
    }
}

QList<lb::ComponentId> TruthTableDialog::checkedIds(QListWidget* list) const
{
    QList<lb::ComponentId> ids;
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* item = list->item(row);
        if (item->checkState() == Qt::Checked)
            ids.append(item->data(Qt::UserRole).toUInt());
    }
    return ids;
}

void TruthTableDialog::generate()
{
    std::vector<lb::ComponentId> inputs;
    for (lb::ComponentId id : checkedIds(m_inputs))
        inputs.push_back(id);
    std::vector<lb::ComponentId> outputs;
    for (lb::ComponentId id : checkedIds(m_outputs))
        outputs.push_back(id);

    const lb::TruthTableResult result =
        lb::TruthTableGenerator::generate(m_document->circuit(), inputs, outputs);

    // The generator restores the switches it borrowed, but the canvas still
    // needs to be told to redraw.
    m_document->resimulate();

    m_table->clear();
    m_exportButton->setEnabled(false);

    if (!result.hasTable()) {
        m_message->setStyleSheet(QStringLiteral("color:#B3261E;"));
        m_message->setText(QString::fromStdString(result.message));
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        return;
    }

    m_lastTable = result.table;
    m_message->setStyleSheet(result.status == lb::TruthTableStatus::Oscillating
                                 ? QStringLiteral("color:#B3261E;")
                                 : QStringLiteral("color:#3C4657;"));
    m_message->setText(result.message.empty()
                           ? tr("%1 rows.").arg(result.table.rowCount())
                           : QString::fromStdString(result.message));

    const int columns = static_cast<int>(result.table.inputNames.size()
                                         + result.table.outputNames.size());
    m_table->setColumnCount(columns);
    m_table->setRowCount(static_cast<int>(result.table.rowCount()));

    QStringList headers;
    for (const std::string& name : result.table.inputNames)
        headers << QString::fromStdString(name);
    for (const std::string& name : result.table.outputNames)
        headers << QString::fromStdString(name);
    m_table->setHorizontalHeaderLabels(headers);

    const int inputCount = static_cast<int>(result.table.inputNames.size());
    for (int row = 0; row < static_cast<int>(result.table.rowCount()); ++row) {
        for (int column = 0; column < columns; ++column) {
            const bool isInput = column < inputCount;
            const lb::Signal value = isInput
                                         ? result.table.inputs[row][column]
                                         : result.table.outputs[row][column - inputCount];
            auto* cell = new QTableWidgetItem(QString(QChar(lb::toChar(value))));
            cell->setTextAlignment(Qt::AlignCenter);
            if (!isInput)
                cell->setForeground(theme::signalColor(value));
            if (isInput)
                cell->setBackground(QColor(0xF5, 0xF7, 0xFA));
            m_table->setItem(row, column, cell);
        }
    }
    m_table->resizeColumnsToContents();
    m_exportButton->setEnabled(true);
}

void TruthTableDialog::exportCsv()
{
    const QString suggestion = m_document->filePath().isEmpty()
                                   ? QStringLiteral("truth_table.csv")
                                   : m_document->filePath().section(QLatin1Char('.'), 0, -2)
                                         + QStringLiteral("_truth_table.csv");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export truth table"), suggestion,
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;

    std::ofstream out(path.toStdString(), std::ios::trunc);
    if (!out) {
        QMessageBox::warning(this, tr("Export failed"), tr("Cannot write %1").arg(path));
        return;
    }
    out << m_lastTable.toCsv();
    m_message->setText(tr("Exported to %1").arg(path));
}

} // namespace lbgui
