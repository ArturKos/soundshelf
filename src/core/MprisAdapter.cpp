#include "soundshelf/core/MprisAdapter.hpp"
#include "soundshelf/core/PlayerEngine.hpp"

#include <QLoggingCategory>

#ifdef SOUNDSHELF_HAVE_MPRIS
#  include <QDBusConnection>
#endif

Q_LOGGING_CATEGORY(lcMpris, "soundshelf.core.mpris")

namespace soundshelf {

MprisAdapter::MprisAdapter(QObject* parent) : QObject(parent) {}

MprisAdapter::~MprisAdapter() {
#ifdef SOUNDSHELF_HAVE_MPRIS
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/mpris/MediaPlayer2"));
    }
#endif
}

bool MprisAdapter::isAvailable() {
#ifdef SOUNDSHELF_HAVE_MPRIS
    return true;
#else
    return false;
#endif
}

Result<void> MprisAdapter::registerService(const QString& serviceName) {
#ifdef SOUNDSHELF_HAVE_MPRIS
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return Result<void>::err(Error::NetworkError,
            QStringLiteral("Cannot connect to D-Bus session bus"));
    }
    if (!bus.registerService(serviceName)) {
        return Result<void>::err(Error::NetworkError,
            QStringLiteral("Cannot own D-Bus service %1").arg(serviceName));
    }
    if (!bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                            QDBusConnection::ExportAllSlots)) {
        return Result<void>::err(Error::NetworkError,
            QStringLiteral("Cannot register /org/mpris/MediaPlayer2"));
    }
    m_registered = true;
    qCInfo(lcMpris) << "MPRIS service registered as" << serviceName;
    return Result<void>::ok();
#else
    Q_UNUSED(serviceName);
    qCDebug(lcMpris) << "MPRIS not compiled in — registerService() is a no-op";
    return Result<void>::ok();
#endif
}

void MprisAdapter::attachEngine(PlayerEngine* engine) {
    m_engine = engine;
    if (!engine) return;

    connect(engine, &PlayerEngine::stateChanged, this, [this](PlayerState s) {
        QString status;
        switch (s) {
            case PlayerState::Playing:   status = QStringLiteral("Playing"); break;
            case PlayerState::Paused:    status = QStringLiteral("Paused"); break;
            case PlayerState::Stopped:
            case PlayerState::Buffering: status = QStringLiteral("Stopped"); break;
        }
        publishPlaybackStatus(status);
    });
    connect(engine, &PlayerEngine::trackChanged, this, &MprisAdapter::publishMetadata);

    connect(this, &MprisAdapter::remoteAction, engine, [engine](const QString& action) {
        if      (action == QLatin1String("Play"))      engine->resume();
        else if (action == QLatin1String("Pause"))     engine->pause();
        else if (action == QLatin1String("PlayPause")) {
            if (engine->state() == PlayerState::Playing) engine->pause();
            else                                          engine->resume();
        }
        else if (action == QLatin1String("Stop"))      engine->stop();
        // Next / Previous are delegated to PlaylistManager via the UI.
    });
}

void MprisAdapter::publishMetadata(const Track& t) {
    Q_UNUSED(t);
    // A full implementation emits PropertiesChanged on
    // org.freedesktop.DBus.Properties for the Metadata key.
    // The structure is a {string, variant} dict on
    // org.mpris.MediaPlayer2.Player.
}

void MprisAdapter::publishPlaybackStatus(const QString& status) {
#ifdef SOUNDSHELF_HAVE_MPRIS
    qCDebug(lcMpris) << "PlaybackStatus =>" << status;
#else
    Q_UNUSED(status);
#endif
}

} // namespace soundshelf
