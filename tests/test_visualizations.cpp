#include <QtTest>
#include <QVector>
#include <QRectF>
#include <QPolygonF>
#include <QPointF>
#include <cstdint>
#include <cmath>

#include "soundshelf/plugins/OscilloscopePlugin.hpp"
#include "soundshelf/plugins/WaveformOverviewPlugin.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

using namespace soundshelf;

namespace {

/// Build a mono S16LE PcmBuffer filled with @p value in every sample.
PcmDecoder::PcmBuffer makeMonoBuf(int frames, int16_t value)
{
    PcmDecoder::PcmBuffer buf;
    buf.sampleRate   = 44100;
    buf.channels     = 1;
    buf.totalSamples = frames;
    buf.s16le.resize(frames * static_cast<int>(sizeof(int16_t)));
    auto* p = reinterpret_cast<int16_t*>(buf.s16le.data());
    for (int i = 0; i < frames; ++i)
        p[i] = value;
    return buf;
}

/// Build a stereo S16LE PcmBuffer where L=@p left, R=@p right for all frames.
PcmDecoder::PcmBuffer makeStereoConstBuf(int frames, int16_t left, int16_t right)
{
    PcmDecoder::PcmBuffer buf;
    buf.sampleRate   = 44100;
    buf.channels     = 2;
    buf.totalSamples = frames;
    buf.s16le.resize(frames * 2 * static_cast<int>(sizeof(int16_t)));
    auto* p = reinterpret_cast<int16_t*>(buf.s16le.data());
    for (int i = 0; i < frames; ++i) {
        p[i * 2 + 0] = left;
        p[i * 2 + 1] = right;
    }
    return buf;
}

} // namespace

class TestVisualizations : public QObject {
    Q_OBJECT

private slots:

    // ── OscilloscopePlugin::buildPolyline ─────────────────────────────────

    /// n < 2 → empty polygon
    void buildPolyline_tooFewSamples()
    {
        const QRectF area(0, 0, 200, 100);
        QVERIFY(OscilloscopePlugin::buildPolyline({}, area).isEmpty());
        QVERIFY(OscilloscopePlugin::buildPolyline({0.5f}, area).isEmpty());
    }

    /// Returned polygon has exactly n points for a valid input.
    void buildPolyline_pointCount()
    {
        const QRectF area(0, 0, 300, 100);
        QVector<float> pcm(64, 0.0f);
        const auto poly = OscilloscopePlugin::buildPolyline(pcm, area);
        QCOMPARE(poly.size(), 64);
    }

    /// First point x == area.left(), last point x == area.right().
    void buildPolyline_xEndpoints()
    {
        const QRectF area(10.0, 5.0, 200.0, 80.0);
        QVector<float> pcm(4, 0.0f);  // 4 samples
        const auto poly = OscilloscopePlugin::buildPolyline(pcm, area);
        QCOMPARE(poly.size(), 4);
        QCOMPARE(poly.first().x(), area.left());
        QCOMPARE(poly.last().x(),  area.right());
    }

    /// y == area.center().y() for zero-amplitude samples.
    void buildPolyline_zeroAmplitudeAtCenter()
    {
        const QRectF area(0.0, 0.0, 100.0, 60.0);
        const QVector<float> pcm = {0.0f, 0.0f, 0.0f};
        const auto poly = OscilloscopePlugin::buildPolyline(pcm, area);
        QCOMPARE(poly.size(), 3);
        for (const QPointF& pt : poly)
            QCOMPARE(pt.y(), area.center().y());
    }

    /// Samples outside [−1, 1] are clamped: +2 maps to area.top(),
    /// −2 maps to area.bottom().
    void buildPolyline_clamping()
    {
        const QRectF area(0.0, 0.0, 100.0, 100.0);
        const QVector<float> pcm = {2.0f, -2.0f};
        const auto poly = OscilloscopePlugin::buildPolyline(pcm, area);
        QCOMPARE(poly.size(), 2);
        // +2 clamped to +1: y = cy − 1*halfH = 50 − 50 = 0 == area.top()
        QCOMPARE(poly[0].y(), area.top());
        // −2 clamped to −1: y = cy − (−1)*halfH = 50 + 50 = 100 == area.bottom()
        QCOMPARE(poly[1].y(), area.bottom());
    }

    /// +1 sample → area.top(), −1 sample → area.bottom().
    void buildPolyline_fullAmplitudeReachesEdges()
    {
        const QRectF area(0.0, 10.0, 100.0, 80.0);
        const QVector<float> pcm = {1.0f, -1.0f};
        const auto poly = OscilloscopePlugin::buildPolyline(pcm, area);
        QCOMPARE(poly.size(), 2);
        QCOMPARE(poly[0].y(), area.top());
        QCOMPARE(poly[1].y(), area.bottom());
    }

    // ── WaveformOverviewPlugin::computeEnvelope ───────────────────────────

    /// Empty s16le buffer → empty result.
    void computeEnvelope_emptyBuffer()
    {
        PcmDecoder::PcmBuffer empty;
        QVERIFY(WaveformOverviewPlugin::computeEnvelope(empty, 100).isEmpty());
    }

    /// totalSamples == 0 → empty result even if bins > 0.
    void computeEnvelope_zeroSamples()
    {
        PcmDecoder::PcmBuffer buf;
        buf.sampleRate   = 44100;
        buf.channels     = 1;
        buf.totalSamples = 0;
        buf.s16le.clear();
        QVERIFY(WaveformOverviewPlugin::computeEnvelope(buf, 10).isEmpty());
    }

    /// bins <= 0 → empty result.
    void computeEnvelope_zeroBins()
    {
        const auto buf = makeMonoBuf(1000, 10000);
        QVERIFY(WaveformOverviewPlugin::computeEnvelope(buf, 0).isEmpty());
        QVERIFY(WaveformOverviewPlugin::computeEnvelope(buf, -1).isEmpty());
    }

    /// Result size equals requested bin count.
    void computeEnvelope_binCount()
    {
        const auto buf = makeMonoBuf(4410, 16000);
        QCOMPARE(WaveformOverviewPlugin::computeEnvelope(buf, 100).size(), 100);
        QCOMPARE(WaveformOverviewPlugin::computeEnvelope(buf, 1).size(),   1);
        QCOMPARE(WaveformOverviewPlugin::computeEnvelope(buf, 4410).size(), 4410);
    }

    /// min ≤ max for every bin.
    void computeEnvelope_minLeMax()
    {
        // Mix of positive and negative values
        PcmDecoder::PcmBuffer buf;
        buf.sampleRate   = 44100;
        buf.channels     = 1;
        buf.totalSamples = 200;
        buf.s16le.resize(200 * static_cast<int>(sizeof(int16_t)));
        auto* p = reinterpret_cast<int16_t*>(buf.s16le.data());
        for (int i = 0; i < 200; ++i)
            p[i] = static_cast<int16_t>((i % 2 == 0) ? 10000 : -10000);

        const auto env = WaveformOverviewPlugin::computeEnvelope(buf, 20);
        QCOMPARE(env.size(), 20);
        for (int b = 0; b < env.size(); ++b)
            QVERIFY2(env[b].min <= env[b].max,
                     qPrintable(QString("bin %1: min=%2 > max=%3")
                                .arg(b).arg(env[b].min).arg(env[b].max)));
    }

    /// Constant-positive mono signal: every bin should have min ≈ max ≈ +1.
    void computeEnvelope_constantPositive()
    {
        // +32767 mono → mono float = 32767 / 32768 ≈ +1
        const auto buf = makeMonoBuf(1000, 32767);
        const auto env = WaveformOverviewPlugin::computeEnvelope(buf, 10);
        QCOMPARE(env.size(), 10);
        for (int b = 0; b < env.size(); ++b) {
            QVERIFY2(env[b].max > 0.99f,
                     qPrintable(QString("bin %1 max=%2 expected ~+1").arg(b).arg(env[b].max)));
            QVERIFY2(env[b].min > 0.99f,
                     qPrintable(QString("bin %1 min=%2 expected ~+1").arg(b).arg(env[b].min)));
        }
    }

    /// Stereo L=+32767, R=−32767 → mono = 0 → envelope all zeros.
    void computeEnvelope_stereoDownmixCancels()
    {
        const auto buf = makeStereoConstBuf(1000, 32767, -32767);
        const auto env = WaveformOverviewPlugin::computeEnvelope(buf, 10);
        QCOMPARE(env.size(), 10);
        for (int b = 0; b < env.size(); ++b) {
            QVERIFY2(std::abs(env[b].min) < 1e-4f,
                     qPrintable(QString("bin %1 min=%2 should be ~0").arg(b).arg(env[b].min)));
            QVERIFY2(std::abs(env[b].max) < 1e-4f,
                     qPrintable(QString("bin %1 max=%2 should be ~0").arg(b).arg(env[b].max)));
        }
    }

    // ── WaveformOverviewPlugin::xToMs ─────────────────────────────────────

    /// Left edge of area → 0 ms.
    void xToMs_leftEdge()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::xToMs(area.left(), area, 60000), 0);
    }

    /// Right edge of area → durationMs.
    void xToMs_rightEdge()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::xToMs(area.right(), area, 60000), 60000);
    }

    /// x below area.left() clamped to 0.
    void xToMs_clampBelow()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::xToMs(0.0, area, 60000), 0);
    }

    /// x above area.right() clamped to durationMs.
    void xToMs_clampAbove()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::xToMs(300.0, area, 60000), 60000);
    }

    /// durationMs <= 0 → always 0.
    void xToMs_zeroDuration()
    {
        const QRectF area(0.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::xToMs(100.0, area, 0), 0);
        QCOMPARE(WaveformOverviewPlugin::xToMs(100.0, area, -1), 0);
    }

    // ── WaveformOverviewPlugin::msToX ─────────────────────────────────────

    /// 0 ms → area.left().
    void msToX_leftEdge()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::msToX(0, area, 60000), area.left());
    }

    /// durationMs → area.right().
    void msToX_rightEdge()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::msToX(60000, area, 60000), area.right());
    }

    /// ms < 0 clamped to area.left().
    void msToX_clampBelow()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::msToX(-1, area, 60000), area.left());
    }

    /// ms > durationMs clamped to area.right().
    void msToX_clampAbove()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::msToX(99999, area, 60000), area.right());
    }

    /// durationMs <= 0 → always area.left().
    void msToX_zeroDuration()
    {
        const QRectF area(10.0, 0.0, 200.0, 50.0);
        QCOMPARE(WaveformOverviewPlugin::msToX(30000, area, 0), area.left());
        QCOMPARE(WaveformOverviewPlugin::msToX(30000, area, -5), area.left());
    }

    /// Round-trip: xToMs(msToX(ms)) ≈ ms (within 1 ms due to integer cast).
    void roundTrip_msToXToMs()
    {
        const QRectF area(0.0, 0.0, 1000.0, 100.0);
        const int D = 180000; // 3 minutes
        for (int ms : {0, 1000, 30000, 90000, 180000}) {
            const double x   = WaveformOverviewPlugin::msToX(ms, area, D);
            const int    ms2 = WaveformOverviewPlugin::xToMs(x, area, D);
            QVERIFY2(std::abs(ms2 - ms) <= 1,
                     qPrintable(QString("ms=%1 → x=%2 → ms2=%3").arg(ms).arg(x).arg(ms2)));
        }
    }

    /// Midpoint x maps to approximately durationMs/2.
    void xToMs_midpoint()
    {
        const QRectF area(0.0, 0.0, 400.0, 80.0);
        const int D   = 120000;
        const int mid = WaveformOverviewPlugin::xToMs(area.center().x(), area, D);
        QVERIFY2(std::abs(mid - D / 2) <= 1,
                 qPrintable(QString("midpoint ms=%1, expected=%2").arg(mid).arg(D / 2)));
    }
};

QTEST_GUILESS_MAIN(TestVisualizations)
#include "test_visualizations.moc"
