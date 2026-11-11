#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace lb {
class BlockLibrary;
}

namespace lbgui {

/// Block library (FR-6): every packaged block, kept in the library folder and
/// shown in the palette in every session. Import, Rename and Delete work on the
/// files through lb::BlockLibrary; deleting a block another block uses asks
/// first.
class BlockLibraryDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlockLibraryDialog(lb::BlockLibrary* library, QWidget* parent = nullptr);

    /// Selects a block by name, if it is there.
    void selectBlock(const QString& name);

signals:
    /// The library changed; the palette's Blocks group must be rebuilt.
    void libraryChanged();

private slots:
    void showBlock();
    void importBlock();
    void renameBlock();
    void deleteBlock();

private:
    void populate(const QString& select = QString());
    QString currentName() const;
    void report(const QString& title, const QString& message);

    lb::BlockLibrary* m_library;
    QTreeWidget* m_blocks;
    QLabel* m_title;
    QLabel* m_details;
    QPushButton* m_renameButton;
    QPushButton* m_deleteButton;
};

} // namespace lbgui
