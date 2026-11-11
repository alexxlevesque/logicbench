#include "CircuitClipboard.h"

#include "logicbench/CircuitSerializer.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include <vector>

namespace lbgui {

bool CircuitClipboard::copy(const lb::Circuit& circuit, const QList<lb::ComponentId>& ids)
{
    std::vector<lb::ComponentId> wanted(ids.begin(), ids.end());
    const lb::Circuit fragment = circuit.extract(wanted);
    if (fragment.componentCount() == 0)
        return false;

    auto* data = new QMimeData;
    data->setData(QString::fromLatin1(kMimeType),
                  QByteArray::fromStdString(lb::CircuitSerializer::toJson(fragment, -1)));
    QGuiApplication::clipboard()->setMimeData(data);
    return true;
}

bool CircuitClipboard::hasFragment()
{
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    return data && data->hasFormat(QString::fromLatin1(kMimeType));
}

std::unique_ptr<lb::Circuit> CircuitClipboard::paste()
{
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    if (!data || !data->hasFormat(QString::fromLatin1(kMimeType)))
        return nullptr;
    try {
        return lb::CircuitSerializer::fromJson(data->data(QString::fromLatin1(kMimeType)).toStdString());
    } catch (const std::exception&) {
        return nullptr;          // written by a newer build, or not ours after all
    }
}

} // namespace lbgui
