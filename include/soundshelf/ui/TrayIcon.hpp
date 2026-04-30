#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QMenu;
class QAction;

namespace soundshelf {

class PlayerEngine;

/**
 * @brief System tray icon with a transport-control context menu.
 *
 * Despite its name `TrayIcon` is **not** a `QWidget` — it lives as a
 * `QObject` so it can outlive every window. Items: Play/Pause, Next,
 * Previous, Stop, Show/Hide main window, Quit.
 *
 * Wires its actions to a @ref PlayerEngine via @ref attachEngine. The
 * "Show / Hide" item emits @ref showMainWindowRequested, which the
 * shell connects to its main window's `setVisible`.
 */
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    /// True when the platform reports tray availability.
    static bool isAvailable();

    void attachEngine(PlayerEngine* engine);

    /// Updates the tooltip / menu state for the now-playing track.
    void setNowPlaying(const QString& title, const QString& artist);

signals:
    void showMainWindowRequested();
    void quitRequested();

private:
    QSystemTrayIcon* m_tray = nullptr;
    QMenu*           m_menu = nullptr;
    QAction*         m_playPauseAct = nullptr;
    PlayerEngine*    m_engine = nullptr;
};

} // namespace soundshelf
