#include "soundshelf/core/VisualizationFeeder.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcVis, "soundshelf.vis")

namespace soundshelf {

VisualizationFeeder::VisualizationFeeder(QObject* parent)
    : QObject(parent)
    , m_decoder([](const QString& path) { return PcmDecoder::decodeToS16(path, 44100, 2); })
{
    m_timer = new QTimer(this);
    m_timer->setInterval(33); // ~30 fps
    connect(m_timer, &QTimer::timeout, this, &VisualizationFeeder::onTimerTick);
}

void VisualizationFeeder::setDecoder(DecodeFn fn)
{
    if (fn)
        m_decoder = std::move(fn);
    else
        m_decoder = [](const QString& path) { return PcmDecoder::decodeToS16(path, 44100, 2); };
}

void VisualizationFeeder::attachEngine(PlayerEngine* engine)
{
    Q_ASSERT(engine);
    m_engine = engine;
    connect(engine, &PlayerEngine::trackChanged,
            this, &VisualizationFeeder::onTrackChanged);
    connect(engine, &PlayerEngine::stateChanged,
            this, &VisualizationFeeder::onStateChanged);
    // The feed timer runs continuously once attached. Gating it on
    // stateChanged() is fragile: a `loadfile replace` on a track switch makes
    // mpv emit a transient END_FILE → stateChanged(Stopped) AFTER play()'s
    // Playing, which used to stop the timer with no later Playing to restart it
    // — so the spectrum froze on every track after the first. Keeping the timer
    // always on and deciding the content per-tick from the engine state makes
    // the feeder immune to that event ordering.
    m_timer->start();
}

QVector<float> VisualizationFeeder::monoWindowAt(const PcmDecoder::PcmBuffer& pcm,
                                                   int positionMs,
                                                   int windowSamples)
{
    QVector<float> result(windowSamples, 0.0f);
    if (windowSamples <= 0)
        return result;
    if (pcm.s16le.isEmpty() || pcm.channels <= 0 || pcm.sampleRate <= 0 || positionMs < 0)
        return result;

    const int64_t frameStart = static_cast<int64_t>(positionMs) * pcm.sampleRate / 1000;
    if (frameStart >= pcm.totalSamples)
        return result;

    const auto* data = reinterpret_cast<const int16_t*>(pcm.s16le.constData());
    const int ch = pcm.channels;
    const float scale = 1.0f / (static_cast<float>(ch) * 32768.0f);

    for (int i = 0; i < windowSamples; ++i) {
        const int64_t frame = frameStart + i;
        if (frame >= pcm.totalSamples)
            break; // zero-pad the rest
        float mono = 0.0f;
        for (int c = 0; c < ch; ++c)
            mono += static_cast<float>(data[frame * ch + c]);
        result[i] = mono * scale;
    }

    return result;
}

void VisualizationFeeder::setWindowSamples(int n)
{
    m_windowSamples = (n > 0) ? n : 1024;
}

int VisualizationFeeder::windowSamples() const
{
    return m_windowSamples;
}

void VisualizationFeeder::onTrackChanged(const soundshelf::Track& track)
{
    clearBuffer();
    if (track.filepath.isEmpty())
        return;
    startDecoding(track.filepath);
}

void VisualizationFeeder::onStateChanged(soundshelf::PlayerState state)
{
    // Track state in our own member (not PlayerEngine::state()) so the feed is
    // driven by the same signal the tests emit, and push an immediate silence
    // frame on pause/stop so the bars drop without waiting for the next tick.
    m_lastState = state;
    if (state == PlayerState::Paused || state == PlayerState::Stopped)
        pushSilence();
}

void VisualizationFeeder::onTimerTick()
{
    if (!m_engine)
        return;
    // Only render live audio while Playing with a decoded buffer; otherwise feed
    // silence so the visualiser reads zero (paused/stopped, or the new track's
    // decode still in flight right after a switch).
    if (m_lastState != PlayerState::Playing || m_pcmBuffer.s16le.isEmpty()) {
        pushSilence();
        return;
    }
    const auto win = monoWindowAt(m_pcmBuffer, m_engine->positionMs(), m_windowSamples);
    m_engine->pushVisualizationPcm(win);
}

void VisualizationFeeder::startDecoding(const QString& path)
{
    ++m_decodeEpoch;
    const int epoch = m_decodeEpoch;

    // Copy the decoder so the lambda owns it independently of the feeder's
    // lifetime (the worker thread outlives the QFutureWatcher cleanup).
    auto dec = m_decoder;
    auto future = QtConcurrent::run([path, dec = std::move(dec)]() -> Result<PcmDecoder::PcmBuffer> {
        return dec(path);
    });

    auto* watcher = new QFutureWatcher<Result<PcmDecoder::PcmBuffer>>(this);
    connect(watcher, &QFutureWatcher<Result<PcmDecoder::PcmBuffer>>::finished,
            this, [this, watcher, epoch]() {
        watcher->deleteLater();
        if (epoch != m_decodeEpoch) {
            qCDebug(lcVis) << "Discarding stale decode (epoch" << epoch
                           << "vs current" << m_decodeEpoch << ')';
            return;
        }
        const auto res = watcher->result();
        if (!res) {
            qCWarning(lcVis) << "PCM decode failed:" << res.error().message;
            return;
        }
        m_pcmBuffer = res.value();
        qCDebug(lcVis) << "PCM ready:" << m_pcmBuffer.totalSamples
                       << "frames @" << m_pcmBuffer.sampleRate << "Hz,"
                       << m_pcmBuffer.channels << "ch";
    });
    watcher->setFuture(future);
}

void VisualizationFeeder::clearBuffer()
{
    ++m_decodeEpoch; // invalidate any in-flight decode
    m_pcmBuffer = {};
}

void VisualizationFeeder::pushSilence()
{
    if (!m_engine)
        return;
    m_engine->pushVisualizationPcm(QVector<float>(m_windowSamples, 0.0f));
}

} // namespace soundshelf
