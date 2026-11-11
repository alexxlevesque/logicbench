#include "AppSettings.h"

#include <QDir>

#include <algorithm>

namespace lbgui {

namespace {
const QString kTickInterval = QStringLiteral("simulation/tickIntervalMs");
const QString kSnap = QStringLiteral("canvas/snapToGrid");
const QString kShowExamples = QStringLiteral("startup/showExamples");
const QString kQuickStartDone = QStringLiteral("startup/quickStartDone");
const QString kLibrary = QStringLiteral("blocks/libraryDirectory");
const QString kLastDirectory = QStringLiteral("files/lastDirectory");
} // namespace

AppSettings::AppSettings()
    : m_settings(std::make_unique<QSettings>())
{
}

AppSettings::AppSettings(const QString& iniPath)
    : m_settings(std::make_unique<QSettings>(iniPath, QSettings::IniFormat))
{
}

int AppSettings::tickIntervalMs() const
{
    return std::clamp(m_settings->value(kTickInterval, 40).toInt(), 5, 1000);
}

void AppSettings::setTickIntervalMs(int ms)
{
    m_settings->setValue(kTickInterval, std::clamp(ms, 5, 1000));
}

bool AppSettings::snapToGrid() const { return m_settings->value(kSnap, true).toBool(); }
void AppSettings::setSnapToGrid(bool on) { m_settings->setValue(kSnap, on); }

bool AppSettings::showExamplesAtStartup() const { return m_settings->value(kShowExamples, true).toBool(); }
void AppSettings::setShowExamplesAtStartup(bool on) { m_settings->setValue(kShowExamples, on); }

bool AppSettings::quickStartDone() const { return m_settings->value(kQuickStartDone, false).toBool(); }
void AppSettings::setQuickStartDone(bool on) { m_settings->setValue(kQuickStartDone, on); }

QString AppSettings::blockLibraryDirectory() const
{
    const QString fallback = QDir(QDir::homePath()).filePath(QStringLiteral("LogicBench/blocks"));
    return m_settings->value(kLibrary, fallback).toString();
}

void AppSettings::setBlockLibraryDirectory(const QString& path) { m_settings->setValue(kLibrary, path); }

QString AppSettings::lastDirectory() const
{
    return m_settings->value(kLastDirectory, QDir::homePath()).toString();
}

void AppSettings::setLastDirectory(const QString& path) { m_settings->setValue(kLastDirectory, path); }

void AppSettings::sync() { m_settings->sync(); }

} // namespace lbgui
