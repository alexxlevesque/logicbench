#include "PaletteDock.h"

#include "CircuitScene.h"

#include "logicbench/ComponentFactory.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QMimeData>

namespace lbgui {

namespace {
constexpr int kPayloadRole = Qt::UserRole + 1;
} // namespace

PaletteTree::PaletteTree(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setIndentation(12);
    setUniformRowHeights(true);
}

QMimeData* PaletteTree::mimeData(const QList<QTreeWidgetItem*>& items) const
{
    if (items.isEmpty())
        return nullptr;
    const QString payload = items.first()->data(0, kPayloadRole).toString();
    if (payload.isEmpty())
        return nullptr;

    auto* data = new QMimeData;
    data->setData(CircuitScene::componentMimeType(), payload.toUtf8());
    data->setText(payload);
    return data;
}

PaletteDock::PaletteDock(QWidget* parent)
    : QDockWidget(tr("Components"), parent)
    , m_tree(new PaletteTree(this))
{
    setObjectName(QStringLiteral("paletteDock"));
    setWidget(m_tree);
    populate();

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) {
                const QString payload = item->data(0, kPayloadRole).toString();
                if (!payload.isEmpty())
                    emit typeActivated(payload);
            });
}

void PaletteDock::populate()
{
    m_tree->clear();
    const lb::ComponentFactory& factory = lb::ComponentFactory::standard();

    for (const std::string& category : factory.categories()) {
        const QString title = QString::fromStdString(category);
        auto* group = new QTreeWidgetItem(m_tree, QStringList{title});
        group->setFlags(Qt::ItemIsEnabled);
        QFont font = group->font(0);
        font.setBold(true);
        group->setFont(0, font);

        for (const lb::ComponentDescriptor& descriptor : factory.inCategory(category)) {
            // The empty subcircuit placeholder is not something to drag out of
            // the palette: a block comes from a file or from packaging.
            if (descriptor.typeName == "SUBCIRCUIT") {
                m_blocks = group;
                continue;
            }
            auto* entry = new QTreeWidgetItem(
                group, QStringList{QString::fromStdString(descriptor.displayName)});
            entry->setData(0, kPayloadRole, QString::fromStdString(descriptor.typeName));
            entry->setToolTip(0, QString::fromStdString(descriptor.summary));
        }
        group->setExpanded(true);
    }

    if (!m_blocks) {
        m_blocks = new QTreeWidgetItem(m_tree, QStringList{tr("Blocks")});
        m_blocks->setFlags(Qt::ItemIsEnabled);
        QFont font = m_blocks->font(0);
        font.setBold(true);
        m_blocks->setFont(0, font);
    }
    m_blocks->setExpanded(true);
}

void PaletteDock::addBlock(const QString& name, const QString& path)
{
    for (int i = 0; i < m_blocks->childCount(); ++i) {
        if (m_blocks->child(i)->text(0) == name)
            return;
    }
    auto* entry = new QTreeWidgetItem(m_blocks, QStringList{name});
    // The payload carries the file the block comes from, so dropping it loads
    // the circuit and wraps it.
    entry->setData(0, kPayloadRole, QStringLiteral("@block:") + path);
    entry->setToolTip(0, QFileInfo(path).absoluteFilePath());
    m_blocks->setExpanded(true);
}

} // namespace lbgui
