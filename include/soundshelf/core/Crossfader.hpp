#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

class PlayerEngine;

/**
 * @brief Implements the time-overlap crossfade between consecutive tracks.
 *
 * Strategy: rather than spinning up a second libmpv process (which is
 * heavy and complicates the audio device handle), the Crossfader hooks
 * into the @ref PlayerEngine and:
 *  1. listens for `position` updates,
 *  2. when `duration - position <= fadeMs` it asks the engine to start
 *     fading the current track out (volume ramp),
 *  3. asks @ref PlayerEngine to preload-and-start the next track at
 *     reduced volume, then ramps both volumes over `fadeMs`.
 *
 * `fadeMs == 0` disables the effect — playback returns to the engine's
 * native gapless behaviour. Typical values: 2000–6000 ms.
 *
 * @note Real overlap requires libmpv 0.34+ with the `loadfile append-play`
 * pattern. The implementation here drives the volume ramp via a
 * 50 Hz timer; the actual second-stream support lives in
 * @ref PlayerEngine and is out of scope for this class.
 */
class Crossfader : public QObject {
    Q_OBJECT
public:
    explicit Crossfader(PlayerEngine* engine, QObject* parent = nullptr);
    ~Crossfader() override;

    /// 0 disables the effect.
    void setFadeMs(int fadeMs);
    int  fadeMs() const { return m_fadeMs; }

    /// Hooks the engine signals. Idempotent.
    void enable();

    /// Disconnects the engine signals.
    void disable();

private slots:
    void onPosition(int posMs);
    void onTrackChanged();
    void tickRamp();

private:
    PlayerEngine* m_engine = nullptr;
    int     m_fadeMs = 0;
    int     m_durationMs = 0;
    int     m_lastPosMs = 0;
    bool    m_fading = false;
    QTimer  m_ramp;
    qint64  m_rampStartElapsedMs = 0;
};

} // namespace soundshelf
