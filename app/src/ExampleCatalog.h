#pragma once

#include <QString>
#include <QVector>

namespace lbgui {

/// One bundled example.
struct ExampleEntry {
    QString file;          ///< file name inside the examples folder
    QString title;
    QString description;
};

/// The bundled examples, in the order and with the titles and descriptions
/// that examples/catalog.json gives them.
///
/// If the catalog is missing or cannot be read, every .lbc in the folder is
/// listed by file name instead, so a broken catalog never hides the examples.
class ExampleCatalog {
public:
    explicit ExampleCatalog(QString directory);

    const QVector<ExampleEntry>& entries() const { return m_entries; }

    /// Full path of an entry's file, or empty for a bad index.
    QString pathFor(int index) const;

    const QString& directory() const { return m_directory; }

private:
    void load();

    QString m_directory;
    QVector<ExampleEntry> m_entries;
};

} // namespace lbgui
