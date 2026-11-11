#pragma once

#include <QSettings>
#include <QString>

#include <memory>

namespace lbgui {

/// Every persistent preference, behind one class over QSettings: a plist on
/// macOS, the registry on Windows, an INI file on Linux. Nothing else in the
/// application reads QSettings directly, so the keys live in one place.
class AppSettings {
public:
    /// The user's own settings.
    AppSettings();
    /// Settings in an INI file of their own, for tests.
    explicit AppSettings(const QString& iniPath);

    int tickIntervalMs() const;                 ///< 5 to 1000, default 40
    void setTickIntervalMs(int ms);

    bool snapToGrid() const;                    ///< default true
    void setSnapToGrid(bool on);

    bool showExamplesAtStartup() const;         ///< default true
    void setShowExamplesAtStartup(bool on);

    bool quickStartDone() const;                ///< default false
    void setQuickStartDone(bool on);

    /// Default ~/LogicBench/blocks.
    QString blockLibraryDirectory() const;
    void setBlockLibraryDirectory(const QString& path);

    /// Where the last file dialog was, so the next one opens there.
    QString lastDirectory() const;
    void setLastDirectory(const QString& path);

    void sync();

private:
    std::unique_ptr<QSettings> m_settings;
};

} // namespace lbgui
