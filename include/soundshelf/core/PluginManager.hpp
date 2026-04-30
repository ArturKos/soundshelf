#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/plugins/VisualizationPlugin.hpp"

namespace soundshelf {

/**
 * @brief Loads and tracks visualisation plugins.
 *
 * Two flavours are managed:
 *  - **native** — instances of @ref VisualizationPlugin compiled into
 *    the application or shipped as Qt plugins (handled via
 *    `QPluginLoader` against `*.so/dll`).
 *  - **Winamp** — `vis_*.dll` loaded through @ref WinampVisAdapter.
 *
 * Plugins live in `$XDG_DATA_HOME/soundshelf/plugins/` (Linux) or
 * `%APPDATA%/SoundShelf/plugins/` (Windows). The manager scans these
 * directories on @ref scan and exposes the discovered list to the UI.
 *
 * Only one visualisation is active at a time — switch with
 * @ref setActive.
 */
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    /// Standard plugin locations (XDG / APPDATA + the running binary's
    /// `plugins/` neighbour).
    static QStringList defaultPluginPaths();

    /// (Re)scans the registered plugin paths.
    Result<void> scan();

    /// All known plugins (native + Winamp), in scan order.
    QList<VisualizationPlugin*> plugins() const { return m_plugins; }

    /// Currently selected plugin, or nullptr if none.
    VisualizationPlugin* active() const { return m_active; }

    /// Switches the active plugin by id. Pass empty to disable.
    Result<void> setActive(const QString& id);

    /// Adds an extra search path (e.g. set by the user in preferences).
    void addSearchPath(const QString& path);

signals:
    void pluginsChanged();
    void activeChanged(VisualizationPlugin* active);

private:
    QStringList                 m_searchPaths;
    QList<VisualizationPlugin*> m_plugins;
    VisualizationPlugin*        m_active = nullptr;
};

} // namespace soundshelf
