#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>

#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

namespace soundshelf {

/**
 * @brief Feeds live PCM data from decoded audio into the spectrum visualiser.
 *
 * When a track starts playing, decodes the source file fully via
 * io::PcmDecoder on a worker thread (QtConcurrent::run — the GUI thread is
 * never blocked). While the engine is in Playing state, a ~30 fps QTimer
 * slices a mono window aligned with PlayerEngine::positionMs() and calls
 * PlayerEngine::pushVisualizationPcm(), which in turn feeds SpectrumWidget.
 *
 * SpectrumWidget already polls spectrumData() on its own timer; no changes
 * to SpectrumWidget are required.
 */
class VisualizationFeeder : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs a feeder. Call attachEngine() to activate it.
     * @param parent Optional Qt parent.
     */
    explicit VisualizationFeeder(QObject* parent = nullptr);

    /**
     * @brief Wires this feeder to @p engine.
     *
     * Connects to trackChanged (triggers background decode) and stateChanged
     * (starts/stops the 30 fps feed timer). Must be called exactly once after
     * the engine is constructed.
     *
     * @param engine Non-null PlayerEngine to drive. Ownership is NOT transferred.
     */
    void attachEngine(PlayerEngine* engine);

    /**
     * @brief Extracts a mono float window from an S16LE PCM buffer.
     *
     * Pure function — deterministic, no side effects, safe to call from tests
     * without any engine or file I/O.
     *
     * Downmixes interleaved S16LE channels to mono by averaging them, then
     * scales to [-1, 1] (dividing by 32768). The returned vector always has
     * exactly @p windowSamples elements. Frames past the end of the buffer are
     * zero-padded. Returns an all-zero window when @p pcm is empty,
     * @p positionMs is negative, or @p positionMs is at or beyond the end of
     * the buffer.
     *
     * @param pcm           Decoded buffer from io::PcmDecoder::decodeToS16.
     * @param positionMs    Playback position in milliseconds.
     * @param windowSamples Requested mono sample count (output vector size).
     * @return QVector<float> of exactly @p windowSamples values in [-1, 1].
     */
    static QVector<float> monoWindowAt(const PcmDecoder::PcmBuffer& pcm,
                                        int positionMs,
                                        int windowSamples);

    /**
     * @brief Sets the PCM window size pushed to the engine per timer tick.
     * @param n Number of mono samples per frame (default: 1024).
     */
    void setWindowSamples(int n);

    /**
     * @brief Returns the current window sample count.
     */
    int windowSamples() const;

private slots:
    void onTrackChanged(const soundshelf::Track& track);
    void onStateChanged(soundshelf::PlayerState state);
    void onTimerTick();

private:
    void startDecoding(const QString& path);
    void clearBuffer();
    void pushSilence();

    PlayerEngine*         m_engine        = nullptr;
    QTimer*               m_timer         = nullptr;
    PcmDecoder::PcmBuffer m_pcmBuffer;
    int                   m_windowSamples = 1024;
    int                   m_decodeEpoch   = 0;
};

} // namespace soundshelf
