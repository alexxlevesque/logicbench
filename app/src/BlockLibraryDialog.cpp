#include "BlockLibraryDialog.h"

#include "logicbench/BlockLibrary.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace lbgui {

namespace {

QString joined(const std::vector<std::string>& names)
{
    QStringList list;
    for (const std::string& n : names)
        list << QString::fromStdString(n);
    return list.isEmpty() ? QObject::tr("none") : list.join(QStringLiteral(", "));
}

} // namespace

BlockLibraryDialog::BlockLibraryDialog(lb::BlockLibrary* library, QWidget* parent)
    : QDialog(parent)
    , m_library(library)
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
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);

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
    m_renameButton = buttons->addButton(tr("Rename…"), QDialogButtonBox::ActionRole);
    m_deleteButton = buttons->addButton(tr("Delete"), QDialogButtonBox::ActionRole);
    auto* closeButton = buttons->addButton(QDialogButtonBox::Close);
    for (QPushButton* button : {importButton, m_renameButton, m_deleteButton})
        button->setAutoDefault(false);
    closeButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(panels, 1);
    layout->addWidget(buttons);

    connect(m_blocks, &QTreeWidget::currentItemChanged, this, &BlockLibraryDialog::showBlock);
    connect(importButton, &QPushButton::clicked, this, &BlockLibraryDialog::importBlock);
    connect(m_renameButton, &QPushButton::clicked, this, &BlockLibraryDialog::renameBlock);
    connect(m_deleteButton, &QPushButton::clicked, this, &BlockLibraryDialog::deleteBlock);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populate();
}

void BlockLibraryDialog::populate(const QString& select)
{
    m_blocks->clear();
    for (const lb::BlockInfo& block : m_library->list()) {
        auto* item = new QTreeWidgetItem(m_blocks);
        item->setText(0, QString::fromStdString(block.name));
        item->setText(1, tr("%1 in, %2 out").arg(block.inputs.size()).arg(block.outputs.size()));
        item->setForeground(1, QColor(0x7A, 0x82, 0x8C));
        item->setSizeHint(0, QSize(0, 30));
        if (QString::fromStdString(block.name) == select)
            m_blocks->setCurrentItem(item);
    }
    if (!m_blocks->currentItem() && m_blocks->topLevelItemCount() > 0)
        m_blocks->setCurrentItem(m_blocks->topLevelItem(0));
    if (m_blocks->topLevelItemCount() == 0) {
        m_title->setText(tr("No blocks yet"));
        m_details->setText(tr("Build a circuit with labelled switches and LEDs, then choose "
                              "Tools › Package as block. It lands here and in the palette.<br><br>"
                              "Folder: <code>%1</code>")
                               .arg(QString::fromStdString(m_library->directory().string()).toHtmlEscaped()));
    }
    m_renameButton->setEnabled(m_blocks->topLevelItemCount() > 0);
    m_deleteButton->setEnabled(m_blocks->topLevelItemCount() > 0);
}

void BlockLibraryDialog::selectBlock(const QString& name)
{
    populate(name);
}

QString BlockLibraryDialog::currentName() const
{
    const QTreeWidgetItem* item = m_blocks->currentItem();
    return item ? item->text(0) : QString();
}

void BlockLibraryDialog::showBlock()
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    for (const lb::BlockInfo& block : m_library->list()) {
        if (QString::fromStdString(block.name) != name)
            continue;
        m_title->setText(name);
        m_details->setText(
            tr("Input pins: %1<br><br>Output pins: %2<br><br>Contains: %3<br><br>Used by: %4<br><br>"
               "File: <code>%5</code>")
                .arg(joined(block.inputs).toHtmlEscaped(), joined(block.outputs).toHtmlEscaped(),
                     QString::fromStdString(block.contents).toHtmlEscaped(),
                     joined(m_library->usersOf(block.name)).toHtmlEscaped(),
                     QString::fromStdString(block.path.string()).toHtmlEscaped()));
        return;
    }
}

void BlockLibraryDialog::report(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
}

void BlockLibraryDialog::importBlock()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import block"), QString(),
                                                      tr("LogicBench circuits (*.lbc)"));
    if (path.isEmpty())
        return;
    try {
        const std::string name = m_library->importFile(path.toStdString());
        populate(QString::fromStdString(name));
        emit libraryChanged();
    } catch (const std::exception& e) {
        report(tr("Cannot import"), QString::fromUtf8(e.what()));
    }
}

void BlockLibraryDialog::renameBlock()
{
    const QString from = currentName();
    if (from.isEmpty())
        return;
    bool ok = false;
    const QString to = QInputDialog::getText(this, tr("Rename block"), tr("New name for %1:").arg(from),
                                             QLineEdit::Normal, from, &ok).trimmed();
    if (!ok || to.isEmpty() || to == from)
        return;
    try {
        m_library->rename(from.toStdString(), to.toStdString());
        populate(to);
        emit libraryChanged();
    } catch (const std::exception& e) {
        report(tr("Cannot rename"), QString::fromUtf8(e.what()));
    }
}

void BlockLibraryDialog::deleteBlock()
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    const std::vector<std::string> users = m_library->usersOf(name.toStdString());
    const QString question = users.empty()
        ? tr("Delete %1 from the library? Circuits that already contain it keep their copy.").arg(name)
        : tr("%1 is used by %2. Those blocks keep their own copy, but %1 will no longer be in the "
             "palette. Delete it anyway?").arg(name, joined(users));
    if (QMessageBox::question(this, tr("Delete block"), question,
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;
    try {
        m_library->remove(name.toStdString());
        populate();
        emit libraryChanged();
    } catch (const std::exception& e) {
        report(tr("Cannot delete"), QString::fromUtf8(e.what()));
    }
}

} // namespace lbgui
