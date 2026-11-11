#include "ExampleCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace lbgui {

ExampleCatalog::ExampleCatalog(QString directory)
    : m_directory(std::move(directory))
{
    load();
}

void ExampleCatalog::load()
{
    const QDir dir(m_directory);
    QFile file(dir.filePath(QStringLiteral("catalog.json")));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray list = QJsonDocument::fromJson(file.readAll()).object()
                                    .value(QStringLiteral("examples")).toArray();
        for (const QJsonValue& value : list) {
            const QJsonObject item = value.toObject();
            ExampleEntry entry{item.value(QStringLiteral("file")).toString(),
                               item.value(QStringLiteral("title")).toString(),
                               item.value(QStringLiteral("description")).toString()};
            // An entry whose file is not there is left out rather than shown
            // and then failing to open.
            if (!entry.file.isEmpty() && dir.exists(entry.file)) {
                if (entry.title.isEmpty())
                    entry.title = QFileInfo(entry.file).completeBaseName();
                m_entries.append(entry);
            }
        }
    }
    if (!m_entries.isEmpty())
        return;

    for (const QString& name : dir.entryList({QStringLiteral("*.lbc")}, QDir::Files, QDir::Name))
        m_entries.append(ExampleEntry{name, QFileInfo(name).completeBaseName(), QString()});
}

QString ExampleCatalog::pathFor(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return QDir(m_directory).filePath(m_entries[index].file);
}

} // namespace lbgui
