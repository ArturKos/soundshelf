#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

class Crossfader;

/// Stan playera.
enum class PlayerState {
    Stopped,
    Playing,
    Paused,
    Buffering
};

/// Tryb pętli.
enum class RepeatMode {
    Off,
    Track,
    All
};

/// Fasada nad libmpv. Oddziela UI od konkretnego backendu audio.
/// Wszystkie operacje są nieblokujące — zwracają natychmiast,
/// rezultat przez sygnały.
class PlayerEngine : public QObject {
    Q_OBJECT
public:
    explicit PlayerEngine(QObject* parent = nullptr);
    ~PlayerEngine() override;

    Result<void> initialize();

    // Playback control
    Result<void> play(const Track& track);
    Result<void> playFile(const QString& path);
    Result<void> playUrl(const QString& url);
    void pause();
    void resume();
    void stop();
    void seekMs(int positionMs);
    void seekRelative(int deltaMs);

    // Properties
    PlayerState state() const { return m_state; }
    int positionMs() const;
    int durationMs() const;
    double volumePercent() const { return m_volume; }
    void setVolume(double percent);          // 0..100
    bool isMuted() const { return m_muted; }
    void setMuted(bool muted);

    // ReplayGain
    void setReplayGainEnabled(bool enabled);
    void setReplayGainAlbumMode(bool albumMode);
    bool replayGainEnabled() const { return m_rgEnabled; }

    // Equalizer (10 bands, gains in dB, range -12 .. +12)
    static constexpr int EQ_BANDS = 10;
    static constexpr double EQ_FREQS[EQ_BANDS] =
        {60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};
    void setEqualizerEnabled(bool enabled);
    void setEqualizerBand(int band, double gainDb);
    void setEqualizerPreset(const QString& presetName);
    bool equalizerEnabled() const { return m_eqEnabled; }
    QVector<double> equalizerGains() const { return m_eqGains; }
    /// Names (file stems) of the bundled EQ presets, e.g. "rock", "jazz".
    static QStringList availablePresets();

    // Gapless / crossfade
    void setGaplessEnabled(bool enabled);
    void setCrossfadeMs(int ms);             // 0 = off

    // Repeat / shuffle
    void setRepeat(RepeatMode mode);
    void setShuffle(bool shuffle);
    RepeatMode repeat() const { return m_repeat; }
    bool shuffle() const { return m_shuffle; }

    // Spectrum data — returns last-captured PCM converted to log-bin levels.
    /// Returns N float values in 0..1 range, one per visualization bar.
    QVector<float> spectrumData(int bars = 24) const;

    /// Feeds a mono PCM frame (−1..1) for the visualizer to analyse. Normally
    /// driven by the audio tap; exposed so the spectrum is testable/usable
    /// independently of the playback pipeline.
    void pushVisualizationPcm(const QVector<float>& monoPcm);

    /// Pure FFT helper: Hann-windowed magnitude spectrum of @p monoPcm folded
    /// into @p bars logarithmically spaced bands (≈20 Hz–20 kHz), each 0..1.
    /// Returns all-zero bars when FFTW3 is not compiled in.
    static QVector<float> computeSpectrum(const QVector<float>& monoPcm,
                                          int bars,
                                          int sampleRate = 44100);

    /// Builds the mpv @c af property string for the given EQ and ReplayGain state.
    /// Returns an empty string when no filter is active; the caller should then
    /// clear the @c af property (pass @c "" to mpv).
    ///
    /// The returned string is a single @c lavfi=[] node wrapping a comma-separated
    /// libavfilter graph:
    ///   - Per active EQ band (gain != 0):
    ///       @c equalizer=f=<Hz>:width_type=q:width=1.0:gain=<dB>
    ///     Frequencies are taken from EQ_FREQS in order; bands with zero gain
    ///     (@c qFuzzyIsNull) are omitted.
    ///   - ReplayGain / volume correction (@p replayGainDb != 0):
    ///       @c volume=<dB>dB  (formatted to two decimal places, e.g. @c volume=-6.50dB)
    ///     appended at the end of the graph.
    ///
    /// @param eqEnabled     Whether the equalizer is active.
    /// @param eqGains       Per-band gains in dB (EQ_BANDS entries, expected
    ///                      clamped to −12 .. +12 by the caller).
    /// @param replayGainDb  Volume correction in dB for ReplayGain/normalization.
    ///                      Pass 0.0 to omit the volume term.
    /// @return mpv @c af value string, or an empty QString when nothing is active.
    static QString buildAudioFilterChain(bool eqEnabled,
                                         const QVector<double>& eqGains,
                                         double replayGainDb);

signals:
    void stateChanged(PlayerState state);
    void trackChanged(const Track& track);
    void positionChanged(int positionMs);
    void durationChanged(int durationMs);
    void trackEnded(const Track& track, int playedMs, bool completed);
    void volumeChanged(double percent);
    void error(const QString& message);
    /// PCM ready dla wizualizacji (raw float buffer, 44100 Hz mono mix)
    void audioBufferReady(const QVector<float>& pcm);

private:
    void* m_mpv = nullptr;            ///< mpv_handle*
    PlayerState m_state = PlayerState::Stopped;
    Track m_currentTrack;
    qint64 m_trackStartedMs = 0;

    double m_volume = 80.0;
    bool m_muted = false;
    bool m_rgEnabled = true;
    bool m_rgAlbumMode = false;
    bool m_eqEnabled = false;
    QVector<double> m_eqGains;
    bool m_gapless = true;
    int m_crossfadeMs = 0;
    Crossfader* m_crossfader = nullptr;   ///< lazily created; parented to this
    QVector<float> m_visPcm;          ///< last PCM frame for the visualizer
    RepeatMode m_repeat = RepeatMode::Off;
    bool m_shuffle = false;

    QString buildAudioFilterChain() const;
    void applyAudioFilters();
    void rebuildEqualizer();

    // Wakeup i event handling
    void handleMpvEvents();
    static void wakeupCallback(void* ctx);
};

} // namespace soundshelf
