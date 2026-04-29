#include "soundshelf/core/PlayerEngine.hpp"

#include <QDateTime>
#include <QStringList>
#include <QTimer>
#include <QLoggingCategory>

#include <mpv/client.h>

Q_LOGGING_CATEGORY(lcPlayer, "soundshelf.player")

namespace soundshelf {

namespace {

inline mpv_handle* asMpv(void* p) { return reinterpret_cast<mpv_handle*>(p); }

void mpvSetString(mpv_handle* mpv, const char* prop, const QByteArray& value) {
    mpv_set_property_string(mpv, prop, value.constData());
}

} // anonymous

PlayerEngine::PlayerEngine(QObject* parent)
    : QObject(parent)
{
    m_eqGains = QVector<double>(EQ_BANDS, 0.0);
}

PlayerEngine::~PlayerEngine() {
    if (m_mpv) {
        mpv_terminate_destroy(asMpv(m_mpv));
    }
}

Result<void> PlayerEngine::initialize() {
    auto* mpv = mpv_create();
    if (!mpv) {
        return Result<void>::err(Error::DependencyMissing,
            QStringLiteral("mpv_create failed"));
    }
    m_mpv = mpv;

    // Konfiguracja przed initialize()
    mpv_set_option_string(mpv, "video", "no");
    mpv_set_option_string(mpv, "audio-display", "no");
    mpv_set_option_string(mpv, "input-default-bindings", "no");
    mpv_set_option_string(mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(mpv, "osc", "no");
    mpv_set_option_string(mpv, "gapless-audio", m_gapless ? "yes" : "no");
    mpv_set_option_string(mpv, "audio-channels", "stereo");
    mpv_set_option_string(mpv, "keep-open", "yes");
    mpv_set_option_string(mpv, "msg-level", "all=warn");

    // Wakeup callback — w main thread przez Qt event loop
    mpv_set_wakeup_callback(mpv, &PlayerEngine::wakeupCallback, this);

    if (mpv_initialize(mpv) < 0) {
        mpv_destroy(mpv);
        m_mpv = nullptr;
        return Result<void>::err(Error::DependencyMissing,
            QStringLiteral("mpv_initialize failed"));
    }

    // Subskrybuj eventy
    mpv_observe_property(mpv, 0, "time-pos",      MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "duration",      MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause",         MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "idle-active",   MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "volume",        MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "eof-reached",   MPV_FORMAT_FLAG);

    setVolume(m_volume);
    qCInfo(lcPlayer) << "PlayerEngine initialized";
    return Result<void>::ok();
}

void PlayerEngine::wakeupCallback(void* ctx) {
    auto* self = static_cast<PlayerEngine*>(ctx);
    // Wakeup przychodzi z mpv thread — postujemy do GUI thread
    QMetaObject::invokeMethod(self, [self]() {
        self->handleMpvEvents();
    }, Qt::QueuedConnection);
}

void PlayerEngine::handleMpvEvents() {
    if (!m_mpv) return;
    auto* mpv = asMpv(m_mpv);

    while (true) {
        mpv_event* ev = mpv_wait_event(mpv, 0);
        if (!ev || ev->event_id == MPV_EVENT_NONE) break;

        switch (ev->event_id) {
            case MPV_EVENT_PROPERTY_CHANGE: {
                auto* prop = static_cast<mpv_event_property*>(ev->data);
                const QString name = QString::fromLatin1(prop->name);
                if (name == QLatin1String("time-pos") && prop->format == MPV_FORMAT_DOUBLE) {
                    const double pos = *static_cast<double*>(prop->data);
                    emit positionChanged(static_cast<int>(pos * 1000));
                } else if (name == QLatin1String("duration") && prop->format == MPV_FORMAT_DOUBLE) {
                    const double dur = *static_cast<double*>(prop->data);
                    emit durationChanged(static_cast<int>(dur * 1000));
                } else if (name == QLatin1String("pause") && prop->format == MPV_FORMAT_FLAG) {
                    const bool paused = *static_cast<int*>(prop->data) != 0;
                    m_state = paused ? PlayerState::Paused : PlayerState::Playing;
                    emit stateChanged(m_state);
                } else if (name == QLatin1String("eof-reached") && prop->format == MPV_FORMAT_FLAG) {
                    const bool eof = *static_cast<int*>(prop->data) != 0;
                    if (eof && m_state == PlayerState::Playing) {
                        const qint64 played = QDateTime::currentMSecsSinceEpoch() - m_trackStartedMs;
                        emit trackEnded(m_currentTrack, static_cast<int>(played), true);
                    }
                }
                break;
            }
            case MPV_EVENT_END_FILE: {
                m_state = PlayerState::Stopped;
                emit stateChanged(m_state);
                break;
            }
            case MPV_EVENT_LOG_MESSAGE: {
                auto* msg = static_cast<mpv_event_log_message*>(ev->data);
                qCDebug(lcPlayer) << "[mpv]" << msg->prefix << msg->level << msg->text;
                break;
            }
            case MPV_EVENT_SHUTDOWN:
                return;
            default:
                break;
        }
    }
}

Result<void> PlayerEngine::play(const Track& track) {
    if (track.filepath.isEmpty()) {
        return Result<void>::err(Error::InvalidArgument, QStringLiteral("Empty filepath"));
    }
    auto r = playFile(track.filepath);
    if (r) {
        m_currentTrack = track;
        emit trackChanged(track);
    }
    return r;
}

Result<void> PlayerEngine::playFile(const QString& path) {
    if (!m_mpv) {
        return Result<void>::err(Error::DependencyMissing, QStringLiteral("Not initialized"));
    }

    // Apply filter chain (RG, EQ) before loading
    applyAudioFilters();

    const QByteArray pathUtf8 = path.toUtf8();
    const char* args[] = { "loadfile", pathUtf8.constData(), "replace", nullptr };
    if (mpv_command(asMpv(m_mpv), args) < 0) {
        return Result<void>::err(Error::Unknown, QStringLiteral("mpv loadfile failed"));
    }

    m_state = PlayerState::Playing;
    m_trackStartedMs = QDateTime::currentMSecsSinceEpoch();
    emit stateChanged(m_state);
    qCDebug(lcPlayer) << "Playing" << path;
    return Result<void>::ok();
}

Result<void> PlayerEngine::playUrl(const QString& url) {
    return playFile(url);   // libmpv obsługuje URL transparentnie
}

void PlayerEngine::pause() {
    if (!m_mpv) return;
    int pause = 1;
    mpv_set_property(asMpv(m_mpv), "pause", MPV_FORMAT_FLAG, &pause);
}

void PlayerEngine::resume() {
    if (!m_mpv) return;
    int pause = 0;
    mpv_set_property(asMpv(m_mpv), "pause", MPV_FORMAT_FLAG, &pause);
}

void PlayerEngine::stop() {
    if (!m_mpv) return;
    const char* args[] = { "stop", nullptr };
    mpv_command(asMpv(m_mpv), args);
    m_state = PlayerState::Stopped;
    emit stateChanged(m_state);
}

void PlayerEngine::seekMs(int positionMs) {
    if (!m_mpv) return;
    const QByteArray pos = QByteArray::number(positionMs / 1000.0, 'f', 3);
    const char* args[] = { "seek", pos.constData(), "absolute", nullptr };
    mpv_command(asMpv(m_mpv), args);
}

void PlayerEngine::seekRelative(int deltaMs) {
    if (!m_mpv) return;
    const QByteArray pos = QByteArray::number(deltaMs / 1000.0, 'f', 3);
    const char* args[] = { "seek", pos.constData(), "relative", nullptr };
    mpv_command(asMpv(m_mpv), args);
}

int PlayerEngine::positionMs() const {
    if (!m_mpv) return 0;
    double pos = 0;
    mpv_get_property(asMpv(m_mpv), "time-pos", MPV_FORMAT_DOUBLE, &pos);
    return static_cast<int>(pos * 1000);
}

int PlayerEngine::durationMs() const {
    if (!m_mpv) return 0;
    double dur = 0;
    mpv_get_property(asMpv(m_mpv), "duration", MPV_FORMAT_DOUBLE, &dur);
    return static_cast<int>(dur * 1000);
}

void PlayerEngine::setVolume(double percent) {
    m_volume = qBound(0.0, percent, 100.0);
    if (m_mpv) {
        mpv_set_property(asMpv(m_mpv), "volume", MPV_FORMAT_DOUBLE, &m_volume);
    }
    emit volumeChanged(m_volume);
}

void PlayerEngine::setMuted(bool muted) {
    m_muted = muted;
    if (m_mpv) {
        int mu = muted ? 1 : 0;
        mpv_set_property(asMpv(m_mpv), "mute", MPV_FORMAT_FLAG, &mu);
    }
}

void PlayerEngine::setReplayGainEnabled(bool enabled) {
    m_rgEnabled = enabled;
    applyAudioFilters();
}

void PlayerEngine::setReplayGainAlbumMode(bool albumMode) {
    m_rgAlbumMode = albumMode;
    applyAudioFilters();
}

void PlayerEngine::setEqualizerEnabled(bool enabled) {
    m_eqEnabled = enabled;
    applyAudioFilters();
}

void PlayerEngine::setEqualizerBand(int band, double gainDb) {
    if (band < 0 || band >= EQ_BANDS) return;
    m_eqGains[band] = qBound(-12.0, gainDb, 12.0);
    applyAudioFilters();
}

void PlayerEngine::setEqualizerPreset(const QString& name) {
    // Presety zdefiniowane w resources/eq_presets/*.json — TODO: ładowanie
    Q_UNUSED(name);
}

void PlayerEngine::setGaplessEnabled(bool enabled) {
    m_gapless = enabled;
    if (m_mpv) {
        mpv_set_property_string(asMpv(m_mpv), "gapless-audio", enabled ? "yes" : "no");
    }
}

void PlayerEngine::setCrossfadeMs(int ms) {
    m_crossfadeMs = ms;
    // Implementacja crossfade wymaga drugiej instancji mpv — TODO
}

void PlayerEngine::setRepeat(RepeatMode mode) {
    m_repeat = mode;
    if (m_mpv) {
        const char* val = (mode == RepeatMode::Track) ? "inf" : "no";
        mpv_set_property_string(asMpv(m_mpv), "loop-file", val);
    }
}

void PlayerEngine::setShuffle(bool shuffle) {
    m_shuffle = shuffle;
}

QString PlayerEngine::buildAudioFilterChain() const {
    QStringList filters;
    if (m_rgEnabled && m_currentTrack.isValid()) {
        const double gain = m_currentTrack.effectiveReplayGainDb(m_rgAlbumMode);
        if (gain != 0.0) {
            // mpv volume filter, ReplayGain w dB
            filters << QStringLiteral("lavfi=[volume=%1dB]").arg(gain, 0, 'f', 2);
        }
    }
    if (m_eqEnabled) {
        for (int i = 0; i < EQ_BANDS; ++i) {
            if (qFuzzyIsNull(m_eqGains[i])) continue;
            filters << QStringLiteral("lavfi=[equalizer=f=%1:t=q:w=1.0:g=%2]")
                .arg(EQ_FREQS[i], 0, 'f', 0)
                .arg(m_eqGains[i], 0, 'f', 1);
        }
    }
    return filters.join(QChar(','));
}

void PlayerEngine::applyAudioFilters() {
    if (!m_mpv) return;
    const QString chain = buildAudioFilterChain();
    if (chain.isEmpty()) {
        mpv_set_property_string(asMpv(m_mpv), "af", "");
    } else {
        mpv_set_property_string(asMpv(m_mpv), "af", chain.toUtf8().constData());
    }
}

QVector<float> PlayerEngine::spectrumData(int bars) const {
    // TODO: prawdziwy FFT z PCM tap. Na razie placeholder z deklarowanych wartości.
    QVector<float> out(bars, 0.0f);
    return out;
}

} // namespace soundshelf
