#pragma once

#include <QDockWidget>
#include <QString>
#include <QTreeWidget>

namespace lbgui {

/// The palette tree. Overrides mimeData() so a component type can be dragged
/// onto the canvas; the scene creates it through the engine's factory, which
/// is why adding a new component type needs no change here.
class PaletteTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit PaletteTree(QWidget* parent = nullptr);

protected:
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override;
};

class PaletteDock : public QDockWidget {
    Q_OBJECT

public:
    explicit PaletteDock(QWidget* parent = nullptr);

    /// Adds a saved block to the Blocks category so it can be dragged out
    /// again during this session.
    void addBlock(const QString& name, const QString& path);

signals:
    /// Double-click: place the type at the centre of the canvas.
    void typeActivated(const QString& payload);

private:
    void populate();

    PaletteTree* m_tree;
    QTreeWidgetItem* m_blocks = nullptr;
};

} // namespace lbgui
