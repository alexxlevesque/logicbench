#pragma once

#include <QDialog>

class QLabel;
class QTreeWidget;

namespace lbgui {

/// Block library (FR-6): every packaged block, kept in the library folder
/// and shown in the palette in every session.
///
/// Shell for the SDD screen capture, filled with the bundled example blocks.
/// When wired in, the list comes from lb::BlockLibrary, and Import, Rename
/// and Delete call it and refresh the palette.
class BlockLibraryDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlockLibraryDialog(QWidget* parent = nullptr);

signals:
    /// The library changed; the palette's Blocks group must be rebuilt.
    void libraryChanged();

private slots:
    void showBlock();
    void importBlock();
    void renameBlock();
    void deleteBlock();

private:
    void populate();

    QTreeWidget* m_blocks;
    QLabel* m_title;
    QLabel* m_details;
};

} // namespace lbgui
