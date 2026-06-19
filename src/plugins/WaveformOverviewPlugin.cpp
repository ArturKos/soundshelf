#include "soundshelf/plugins/WaveformOverviewPlugin.hpp"
#include "soundshelf/plugins/VisualStyle.hpp"
#include "soundshelf/core/PlayerEngine.hpp"

#include <QPainter>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QtConcurrent>
#include <algorithm>
#include <cstdint>
#include <limits>

Q_LOGGING_CATEGORY(lcWaveform, "soundshelf.plugins.vis")

namespace soundshelf {

WaveformOverviewPlugin::WaveformOverviewPlugin(QObject* parent)
    : VisualizationPlugin(parent)
    , m_decoder([](const QString& path) { return PcmDecoder::decodeToS16(path, 44100, 2); })
{}

WaveformOverviewPlugin::~WaveformOverviewPlugin() = default;

void WaveformOverviewPlugin::setEngine(PlayerEngine* engine)
{
    Q_ASSERT(engine);
    m_engine = engine;
    connect(engine, &PlayerEngine::trackChanged,
            this, &WaveformOverviewPlugin::onTrackChanged);
}

void WaveformOverviewPlugin::setDecoder(DecodeFn fn)
{
    if (fn)
        m_decoder = std::move(fn);
    else
        m_decoder = [](const QString& path) { return PcmDecoder::decodeToS16(path, 44100, 2); };
}

void WaveformOverviewPlugin::handleClick(double x, const QRectF& area)
{
    if (!m_engine || m_durationMs <= 0) return;
    const int ms = xToMs(x, area, m_durationMs);
    m_engine->seekMs(ms);
}

void WaveformOverviewPlugin::render(QPainter& painter,
                                    const QRectF& area,
                                    const QVector<float>& /*pcm*/,
                                    const QVector<float>& /*spectrum*/)
{
    if (area.width() <= 0 || area.height() <= 0) return;

    const qreal cy    = area.center().y();
    const qreal halfH = area.height() / 2.0;

    if (!m_envelope.isEmpty()) {
        const VisualStyle st = currentVisualStyle();
        const int n = m_envelope.size();
        // Fraction of the track already played → that part renders at full
        // brightness, the rest is dimmed so the waveform doubles as a progress bar.
        const qreal playedFrac = (m_engine && m_durationMs > 0)
            ? qBound(0.0, double(m_engine->positionMs()) / m_durationMs, 1.0) : 0.0;
        for (int i = 0; i < n; ++i) {
            const qreal t = (i + 0.5) / n;
            const qreal x = area.left() + (i + 0.5) * area.width() / n;
            const qreal lvl = qMax(std::abs(m_envelope[i].max), std::abs(m_envelope[i].min));
            QColor c = visColor(st, t, qBound(0.3, double(0.35 + 0.65 * lvl), 1.0));
            if (t > playedFrac) c = c.darker(190);   // unplayed → dimmed
            painter.setPen(c);
            const qreal yMax = cy - static_cast<qreal>(m_envelope[i].max) * halfH;
            const qreal yMin = cy - static_cast<qreal>(m_envelope[i].min) * halfH;
            painter.drawLine(QPointF(x, yMax), QPointF(x, yMin));
        }
    } else {
        // Placeholder while the background decode is in progress
        painter.setPen(QColor(30, 60, 30));
        painter.drawLine(QPointF(area.left(), cy), QPointF(area.right(), cy));
    }

    // Playback position cursor
    if (m_engine && m_durationMs > 0) {
        const int posMs  = m_engine->positionMs();
        const qreal curX = msToX(posMs, area, m_durationMs);
        painter.setPen(QPen(QColor(255, 255, 255, 200), 1));
        painter.drawLine(QPointF(curX, area.top()), QPointF(curX, area.bottom()));
    }
}

// ── Pure static helpers ────────────────────────────────────────────────────

QVector<WaveformOverviewPlugin::MinMax>
WaveformOverviewPlugin::computeEnvelope(const PcmDecoder::PcmBuffer& pcm, int bins)
{
    if (bins <= 0 || pcm.s16le.isEmpty() || pcm.totalSamples <= 0 || pcm.channels <= 0)
        return {};

    const auto* data = reinterpret_cast<const int16_t*>(pcm.s16le.constData());
    const int   ch    = pcm.channels;
    const float scale = 1.0f / (32768.0f * static_cast<float>(ch));
    const int   total = pcm.totalSamples;

    QVector<MinMax> result(bins);
    for (int b = 0; b < bins; ++b) {
        const int frameStart = static_cast<int>(
            static_cast<int64_t>(b) * total / bins);
        const int frameEnd = (b == bins - 1)
            ? total
            : static_cast<int>(static_cast<int64_t>(b + 1) * total / bins);

        float mn = std::numeric_limits<float>::max();
        float mx = std::numeric_limits<float>::lowest();
        for (int f = frameStart; f < frameEnd; ++f) {
            float mono = 0.0f;
            for (int c = 0; c < ch; ++c)
                mono += static_cast<float>(data[f * ch + c]);
            mono *= scale;
            if (mono < mn) mn = mono;
            if (mono > mx) mx = mono;
        }
        // Guard against an empty bin (frameStart == frameEnd).
        if (mn > mx) { mn = 0.0f; mx = 0.0f; }
        result[b] = {mn, mx};
    }
    return result;
}

int WaveformOverviewPlugin::xToMs(double x, const QRectF& area, int durationMs)
{
    if (durationMs <= 0) return 0;
    const double clamped = std::clamp(x, area.left(), area.right());
    const double ratio   = (area.width() > 0.0)
        ? (clamped - area.left()) / area.width()
        : 0.0;
    const double ms = ratio * static_cast<double>(durationMs);
    return static_cast<int>(std::clamp(ms, 0.0, static_cast<double>(durationMs)));
}

double WaveformOverviewPlugin::msToX(int ms, const QRectF& area, int durationMs)
{
    if (durationMs <= 0) return area.left();
    const int clamped = std::clamp(ms, 0, durationMs);
    return area.left() + static_cast<double>(clamped) / static_cast<double>(durationMs)
           * area.width();
}

// ── Private slots ──────────────────────────────────────────────────────────

void WaveformOverviewPlugin::onTrackChanged(const soundshelf::Track& track)
{
    clearEnvelope();
    if (track.filepath.isEmpty()) return;
    m_durationMs = track.durationMs;
    startDecoding(track.filepath);
}

void WaveformOverviewPlugin::startDecoding(const QString& path)
{
    ++m_decodeEpoch;
    const int epoch = m_decodeEpoch;

    auto dec    = m_decoder; // copy into lambda so it outlives any setDecoder() call
    auto future = QtConcurrent::run(
        [path, dec = std::move(dec)]() -> Result<PcmDecoder::PcmBuffer> {
            return dec(path);
        });

    auto* watcher = new QFutureWatcher<Result<PcmDecoder::PcmBuffer>>(this);
    connect(watcher, &QFutureWatcher<Result<PcmDecoder::PcmBuffer>>::finished,
            this, [this, watcher, epoch]() {
        watcher->deleteLater();
        if (epoch != m_decodeEpoch) {
            qCDebug(lcWaveform) << "Discarding stale waveform decode (epoch" << epoch
                                << "vs current" << m_decodeEpoch << ')';
            return;
        }
        const auto res = watcher->result();
        if (!res) {
            qCWarning(lcWaveform) << "Waveform decode failed:" << res.error().message;
            return;
        }
        // Use 1024 bins; render() maps them to actual pixel columns on the fly.
        m_envelope = computeEnvelope(res.value(), 1024);
        qCDebug(lcWaveform) << "Waveform envelope ready:" << m_envelope.size() << "bins";
    });
    watcher->setFuture(future);
}

void WaveformOverviewPlugin::clearEnvelope()
{
    ++m_decodeEpoch; // invalidate any in-flight decode
    m_envelope.clear();
    m_durationMs = 0;
}

} // namespace soundshelf
