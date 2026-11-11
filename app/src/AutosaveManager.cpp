#include "AutosaveManager.h"

#include "CircuitDocument.h"

#include "logicbench/CircuitSerializer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QUndoStack>

namespace lbgui {

AutosaveManager::AutosaveManager(CircuitDocument* document, QString directory, QObject* parent)
    : QObject(parent)
    , m_document(document)
    , m_directory(std::move(directory))
{
    m_timer.setInterval(kIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &AutosaveManager::writeRecovery);
    connect(m_document->undoStack(), &QUndoStack::cleanChanged, this, &AutosaveManager::onCleanChanged);
    if (m_document->isModified())
        m_timer.start();
}

QString AutosaveManager::recoveryPath() const
{
    return QDir(m_directory).filePath(QStringLiteral("recovery.lbc"));
}

QString AutosaveManager::metadataPath() const
{
    return QDir(m_directory).filePath(QStringLiteral("recovery.json"));
}

bool AutosaveManager::hasRecovery() const
{
    return QFileInfo::exists(recoveryPath());
}

QDateTime AutosaveManager::recoveryTime() const
{
    QFile file(metadataPath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject meta = QJsonDocument::fromJson(file.readAll()).object();
        const QDateTime saved = QDateTime::fromString(meta.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
        if (saved.isValid())
            return saved;
    }
    return QFileInfo(recoveryPath()).lastModified();
}

QString AutosaveManager::recoverySource() const
{
    QFile file(metadataPath());
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("source")).toString();
}

void AutosaveManager::onCleanChanged(bool clean)
{
    if (clean) {
        // Saved (or undone back to the saved state): nothing left to recover.
        m_timer.stop();
        discardRecovery();
    } else {
        m_timer.start();
    }
}

void AutosaveManager::writeRecovery()
{
    if (!m_document->isModified())
        return;
    QDir().mkpath(m_directory);

    // QSaveFile writes to a temporary and renames, so a crash mid-write leaves
    // the previous recovery file, never half of one.
    QSaveFile circuit(recoveryPath());
    if (!circuit.open(QIODevice::WriteOnly))
        return;
    circuit.write(QByteArray::fromStdString(lb::CircuitSerializer::toJson(m_document->circuit())));
    if (!circuit.commit())
        return;

    QJsonObject meta;
    meta.insert(QStringLiteral("source"), m_document->filePath());
    meta.insert(QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    QSaveFile metadata(metadataPath());
    if (metadata.open(QIODevice::WriteOnly)) {
        metadata.write(QJsonDocument(meta).toJson());
        metadata.commit();
    }
}

void AutosaveManager::discardRecovery()
{
    QFile::remove(recoveryPath());
    QFile::remove(metadataPath());
}

bool AutosaveManager::offerRecovery(QWidget* parent)
{
    if (!hasRecovery())
        return false;

    const QString source = recoverySource();
    const QString when = QLocale().toString(recoveryTime(), QLocale::ShortFormat);
    QMessageBox box(QMessageBox::Question, tr("LogicBench"), tr("LogicBench closed unexpectedly."),
                    QMessageBox::NoButton, parent);
    box.setInformativeText(source.isEmpty()
                               ? tr("Restore the unsaved work from %1?").arg(when)
                               : tr("Restore the unsaved work from %1 (%2)?")
                                     .arg(when, QFileInfo(source).fileName()));
    QPushButton* restore = box.addButton(tr("Restore"), QMessageBox::AcceptRole);
    box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    box.setDefaultButton(restore);
    box.exec();

    if (box.clickedButton() != restore) {
        discardRecovery();
        return false;
    }
    QString error;
    if (!m_document->restoreRecovery(recoveryPath(), source, &error)) {
        QMessageBox::warning(parent, tr("Cannot restore"), error);
        return false;
    }
    return true;
}

} // namespace lbgui
