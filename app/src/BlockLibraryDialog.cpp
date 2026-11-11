#include "BlockLibraryDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace lbgui {

namespace {

struct SampleBlock {
    const char* name;
    const char* inputs;
    const char* outputs;
    int inputCount;
    int outputCount;
    const char* contains;
    const char* usedBy;
};

// The bundled examples as they will sit in the library. lb::BlockLibrary
// replaces this table once FR-6 lands.
const SampleBlock kSamples[] = {
    {"half_adder", "A, B", "C, S", 2, 2, "1 XOR gate, 1 AND gate", "full_adder"},
    {"full_adder", "A, B, Cin", "Cout, S", 3, 2, "2 half_adder blocks, 1 OR gate", "adder4"},
    {"adder4", "A0–A3, B0–B3, Cin", "Cout, S0–S3", 9, 5, "4 full_adder blocks",
     "nothing"},
    {"sr_latch", "R, S", "Q, Qbar", 2, 2, "2 NOR gates", "nothing"},
};

} // namespace

BlockLibraryDialog::BlockLibraryDialog(QWidget* parent)
    : QDialog(parent)
    , m_blocks(new QTreeWidget(this))
    , m_title(new QLabel(this))
    , m_details(new QLabel(this))
{
    setWindowTitle(tr("Block library"));
    resize(600, 340);

    m_blocks->setColumnCount(2);
    m_blocks->setHeaderHidden(true);
    m_blocks->setRootIsDecorated(false);
    m_blocks->header()->setStretchLastSection(false);
    m_blocks->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_blocks->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_blocks->setMinimumWidth(230);

    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_details->setTextFormat(Qt::RichText);
    m_details->setWordWrap(true);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* note = new QLabel(tr("Every block here appears in the palette under Blocks in every "
                               "session. Deleting a block that another block uses asks first."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#7A828C;"));

    auto* detailLayout = new QVBoxLayout;
    detailLayout->addWidget(m_title);
    detailLayout->addWidget(m_details);
    detailLayout->addSpacing(8);
    detailLayout->addWidget(note);
    detailLayout->addStretch(1);

    auto* panels = new QHBoxLayout;
    panels->addWidget(m_blocks, 0);
    panels->addSpacing(12);
    panels->addLayout(detailLayout, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* importButton = buttons->addButton(tr("Import .lbc…"), QDialogButtonBox::ActionRole);
    auto* renameButton = buttons->addButton(tr("Rename…"), QDialogButtonBox::ActionRole);
    auto* deleteButton = buttons->addButton(tr("Delete"), QDialogButtonBox::ActionRole);
    auto* closeButton = buttons->addButton(QDialogButtonBox::Close);
    for (QPushButton* button : {importButton, renameButton, deleteButton})
        button->setAutoDefault(false);
    closeButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(panels, 1);
    layout->addWidget(buttons);

    connect(m_blocks, &QTreeWidget::currentItemChanged, this, &BlockLibraryDialog::showBlock);
    connect(importButton, &QPushButton::clicked, this, &BlockLibraryDialog::importBlock);
    connect(renameButton, &QPushButton::clicked, this, &BlockLibraryDialog::renameBlock);
    connect(deleteButton, &QPushButton::clicked, this, &BlockLibraryDialog::deleteBlock);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populate();
}

void BlockLibraryDialog::populate()
{
    m_blocks->clear();
    for (const SampleBlock& block : kSamples) {
        auto* item = new QTreeWidgetItem(m_blocks);
        item->setText(0, QString::fromUtf8(block.name));
        item->setText(1, tr("%1 in, %2 out").arg(block.inputCount).arg(block.outputCount));
        item->setForeground(1, QColor(0x7A, 0x82, 0x8C));
        item->setSizeHint(0, QSize(0, 30));
    }
    m_blocks->setCurrentItem(m_blocks->topLevelItem(1));
}

void BlockLibraryDialog::showBlock()
{
    const int row = m_blocks->indexOfTopLevelItem(m_blocks->currentItem());
    if (row < 0)
        return;
    const SampleBlock& block = kSamples[row];
    m_title->setText(QString::fromUtf8(block.name));
    m_details->setText(
        tr("Input pins: %1<br><br>Output pins: %2<br><br>Contains: %3<br><br>Used by: %4<br><br>"
           "File: <code>~/LogicBench/blocks/%5.lbc</code>")
            .arg(QString::fromUtf8(block.inputs), QString::fromUtf8(block.outputs),
                 QString::fromUtf8(block.contains), QString::fromUtf8(block.usedBy),
                 QString::fromUtf8(block.name)));
}

void BlockLibraryDialog::importBlock()
{
    // Not wired in yet (FR-6): lb::BlockLibrary::importFile.
}

void BlockLibraryDialog::renameBlock()
{
    // Not wired in yet (FR-6): lb::BlockLibrary::rename.
}

void BlockLibraryDialog::deleteBlock()
{
    // Not wired in yet (FR-6): lb::BlockLibrary::usersOf, then remove.
}

} // namespace lbgui
