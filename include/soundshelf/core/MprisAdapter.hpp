#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

class PlayerEngine;

/**
 * @brief Linux MPRIS2 / D-Bus integration.
 *
 * Exposes the standard `org.mpris.MediaPlayer2` and
 * `org.mpris.MediaPlayer2.Player` interfaces so desktop environments
 * (KDE Plasma, GNOME, sway-bar, playerctl, ...) can drive the player
 * with media keys and display metadata.
 *
 * On non-Linux builds (or when QtDBus is unavailable) the class
 * compiles to no-op stubs — the public API stays the same so callers
 * don't need `#ifdef`s. The compile-time switch is the
 * `SOUNDSHELF_HAVE_MPRIS` macro defined by CMake when `Qt6::DBus` is
 * found.
 *
 * Wire your @ref PlayerEngine via @ref attachEngine. The adapter
 * subscribes to its signals and forwards inbound D-Bus calls (Play,
 * Pause, Next, Previous, Stop, Seek) back to the engine.
 */
class MprisAdapter : public QObject {
    Q_OBJECT
public:
    explicit MprisAdapter(QObject* parent = nullptr);
    ~MprisAdapter() override;

    /// Registers the service on the session bus and returns success.
    /// On platforms without DBus this is a no-op returning ok.
    Result<void> registerService(const QString& serviceName
                                     = QStringLiteral("org.mpris.MediaPlayer2.SoundShelf"));

    /// Bidirectional binding to the player engine.
    void attachEngine(PlayerEngine* engine);

    /// Pushes the currently playing track to subscribers.
    void publishMetadata(const Track& t);

    /// Pushes the playback state ("Playing" / "Paused" / "Stopped").
    void publishPlaybackStatus(const QString& status);

    /// Returns whether the build has real DBus support.
    static bool isAvailable();

signals:
    /// Emitted when a remote MPRIS client requests a control action
    /// such as `Play`, `Pause`, `PlayPause`, `Stop`, `Next`, `Previous`.
    void remoteAction(const QString& action);

private:
    PlayerEngine* m_engine = nullptr;
    bool          m_registered = false;
};

} // namespace soundshelf
