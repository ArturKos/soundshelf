#include <QtTest>
#include <QVector>
#include <atomic>
#include <cmath>

#include "soundshelf/core/VisualizationFeeder.hpp"
#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/core/Result.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

using namespace soundshelf;

/// Thin subclass that lets tests emit PlayerEngine signals without libmpv.
class TestPlayerEngine : public PlayerEngine {
    Q_OBJECT
public:
    using PlayerEngine::PlayerEngine;
    void emitTrackChanged(const Track& t) { emit trackChanged(t); }
    void emitStateChanged(PlayerState s)  { emit stateChanged(s); }
};

namespace {

/// Synthesizes a stereo interleaved S16LE buffer with a sine wave at @p freq Hz.
PcmDecoder::PcmBuffer makeSineBuf(double freq, int sampleRate, int channels, int frames)
{
    PcmDecoder::PcmBuffer buf;
    buf.sampleRate   = sampleRate;
    buf.channels     = channels;
    buf.totalSamples = frames;
    buf.s16le.resize(frames * channels * static_cast<int>(sizeof(int16_t)));

    auto* ptr = reinterpret_cast<int16_t*>(buf.s16le.data());
    for (int f = 0; f < frames; ++f) {
        const int16_t s = static_cast<int16_t>(
            30000.0 * std::sin(2.0 * M_PI * freq * f / sampleRate));
        for (int c = 0; c < channels; ++c)
            ptr[f * channels + c] = s;
    }
    return buf;
}

bool allZero(const QVector<float>& v)
{
    for (float x : v) if (x != 0.0f) return false;
    return true;
}

} // namespace

class TestVisualizationFeeder : public QObject {
    Q_OBJECT

private slots:

    // (1) Synthesized sine buffer → window has expected size and is NOT all-zero.
    void sineWindowHasCorrectSizeAndIsNonZero()
    {
        const auto buf = makeSineBuf(440.0, 44100, 2, 44100);
        const auto win = VisualizationFeeder::monoWindowAt(buf, 100, 1024);
        QCOMPARE(win.size(), 1024);
        QVERIFY2(!allZero(win), "monoWindowAt of a 440 Hz sine should not be all-zero");
    }

    // (2) Push window into engine → spectrumData() has at least one bar > 0 (FFTW3 guard).
    void pushPcmFeedsSpectrumData()
    {
        PlayerEngine engine; // no initialize() — pushVisualizationPcm / spectrumData skip mpv

        const auto buf = makeSineBuf(440.0, 44100, 2, 44100);
        const auto win = VisualizationFeeder::monoWindowAt(buf, 0, 1024);

        engine.pushVisualizationPcm(win);

        const auto spectrum = engine.spectrumData(24);
        QCOMPARE(spectrum.size(), 24);

        // Guard: only assert non-zero if FFTW3 is compiled in (same pattern as test_spectrum).
        if (!PlayerEngine::computeSpectrum(win, 24).isEmpty()) {
            float maxv = 0.0f;
            for (float v : spectrum) maxv = std::max(maxv, v);
            QVERIFY2(maxv > 0.0f,
                     "spectrumData() should have at least one non-zero band after pushVisualizationPcm");
        }
    }

    // (3a) Silent (all-zero) buffer → window is all-zero.
    void silentBufferWindowIsZero()
    {
        PcmDecoder::PcmBuffer buf;
        buf.sampleRate   = 44100;
        buf.channels     = 2;
        buf.totalSamples = 44100;
        buf.s16le.fill(0, 44100 * 2 * static_cast<int>(sizeof(int16_t)));

        const auto win = VisualizationFeeder::monoWindowAt(buf, 0, 1024);
        QCOMPARE(win.size(), 1024);
        QVERIFY(allZero(win));
    }

    // (3b) Position at or beyond the end → all-zero, length still == windowSamples.
    void positionBeyondEndReturnsZeroPadded()
    {
        const auto buf = makeSineBuf(440.0, 44100, 2, 44100); // 1 s

        // 5000 ms is 5 s — well past the 1 s buffer.
        const auto win = VisualizationFeeder::monoWindowAt(buf, 5000, 512);
        QCOMPARE(win.size(), 512);
        QVERIFY(allZero(win));
    }

    // (3c) Empty buffer → all-zero window of the requested size.
    void emptyBufferReturnsZeroWindow()
    {
        PcmDecoder::PcmBuffer empty;
        const auto win = VisualizationFeeder::monoWindowAt(empty, 0, 256);
        QCOMPARE(win.size(), 256);
        QVERIFY(allZero(win));
    }

    // (3d) Window extends past end → first N samples real, rest zero-padded.
    void nearEndWindowIsZeroPadded()
    {
        // 100 frames of mono audio at 44100 Hz
        const auto buf = makeSineBuf(440.0, 44100, 1, 100);

        // Start at position 0 ms → frameStart=0; window=256 > 100 real frames.
        const auto win = VisualizationFeeder::monoWindowAt(buf, 0, 256);
        QCOMPARE(win.size(), 256);

        // Frames 0..99 from the sine are real (non-zero for a non-silent sine).
        bool anyNonZeroInHead = false;
        for (int i = 0; i < 100; ++i)
            if (win[i] != 0.0f) { anyNonZeroInHead = true; break; }
        QVERIFY(anyNonZeroInHead);

        // Frames 100..255 must be zero-padded.
        for (int i = 100; i < 256; ++i)
            QCOMPARE(win[i], 0.0f);
    }

    // (4) Negative position → all-zero window.
    void negativePositionReturnsZero()
    {
        const auto buf = makeSineBuf(440.0, 44100, 2, 44100);
        const auto win = VisualizationFeeder::monoWindowAt(buf, -1, 128);
        QCOMPARE(win.size(), 128);
        QVERIFY(allZero(win));
    }

    // (5) setWindowSamples / windowSamples round-trip.
    void windowSamplesRoundtrip()
    {
        VisualizationFeeder feeder;
        QCOMPARE(feeder.windowSamples(), 1024);
        feeder.setWindowSamples(2048);
        QCOMPARE(feeder.windowSamples(), 2048);
    }

    // (6) D8 regression: spectrum must restart (become non-zero) for BOTH the
    //     first track AND a subsequent track change, not freeze on track A.
    //
    //     The injectable decoder seam removes the ffmpeg dependency so the test
    //     runs without any audio files on disk.
    //
    //     The test proves the restart in two complementary ways:
    //      (a) bFlacDecoded — an atomic flag set inside the decoder lambda when
    //          "B.flac" is requested; QTRY on it verifies a fresh decode was
    //          triggered by onTrackChanged (would fail if onTrackChanged was a no-op).
    //      (b) spectrum non-zero after a verified-zero dip — emitting Stopped stops
    //          the 33ms feed timer AND pushes silence synchronously, guaranteeing
    //          the spectrum is zero before track B starts.  Any subsequent non-zero
    //          reading therefore comes from B's decoded data, not stale A data.
    void trackChangeRestartsSpectrum()
    {
        // Build two distinct synthetic buffers (different frequencies).
        const auto bufA = makeSineBuf(440.0, 44100, 2, 44100);
        const auto bufB = makeSineBuf(880.0, 44100, 2, 44100);

        // Track whether the decoder was asked for "B.flac".
        // Atomic because the decoder runs on a QtConcurrent worker thread.
        std::atomic<bool> bFlacDecoded{false};

        VisualizationFeeder::DecodeFn dec =
            [&bFlacDecoded, bufA, bufB](const QString& path) -> Result<PcmDecoder::PcmBuffer>
        {
            if (path == QLatin1String("A.flac")) return bufA;
            if (path == QLatin1String("B.flac")) {
                bFlacDecoded.store(true, std::memory_order_release);
                return bufB;
            }
            return Result<PcmDecoder::PcmBuffer>::err(
                Error::InvalidArgument, QStringLiteral("unknown test path: %1").arg(path));
        };

        // PlayerEngine without initialize() — libmpv is skipped.
        // pushVisualizationPcm / spectrumData still work (they don't need mpv).
        TestPlayerEngine engine;
        VisualizationFeeder feeder;
        feeder.setDecoder(dec);
        feeder.attachEngine(&engine);

        // FFTW3 guard: same pattern as test case (2).  In practice FFTW3 is
        // always present; the guard keeps the test formally correct if it is not.
        const bool hasFftw = !PlayerEngine::computeSpectrum(
            VisualizationFeeder::monoWindowAt(bufA, 0, 1024), 24).isEmpty();

        auto specNonZero = [&engine]() {
            const auto s = engine.spectrumData(24);
            for (float v : s) if (v > 0.0f) return true;
            return false;
        };

        // ── Track A ───────────────────────────────────────────────────────────
        Track trackA;
        trackA.filepath = QStringLiteral("A.flac");
        engine.emitTrackChanged(trackA);
        engine.emitStateChanged(PlayerState::Playing); // starts the 33ms feed timer

        // Pump the event loop: QFutureWatcher delivers finished(), timer fires.
        if (hasFftw) {
            QTRY_VERIFY_WITH_TIMEOUT(specNonZero(), 5000);
        } else {
            QTest::qWait(300); // still exercises the decode/push cycle
        }

        // ── Verified-zero dip before track B ─────────────────────────────────
        // emitStateChanged(Stopped) triggers onStateChanged which synchronously:
        //   (a) calls m_timer->stop() — no more A-data pushes from the feeder, and
        //   (b) calls pushSilence() — spectrum becomes all-zero immediately.
        // Without this step the 33ms timer would keep pushing A's buffer during any
        // qWait, making it impossible to establish a zero baseline before B starts.
        engine.emitStateChanged(PlayerState::Stopped);
        if (hasFftw) {
            // Deterministic: no timer firing, silence pushed synchronously above.
            QVERIFY(!specNonZero());
        }

        // ── Track B (the regression) ──────────────────────────────────────────
        Track trackB;
        trackB.filepath = QStringLiteral("B.flac");
        engine.emitTrackChanged(trackB);           // clears buffer, starts B decode
        engine.emitStateChanged(PlayerState::Playing); // restart the feed timer for B

        // (a) Prove a fresh decode of B.flac was triggered — this assertion fails
        //     if onTrackChanged is a no-op (the exact frozen-first-track bug).
        QTRY_VERIFY_WITH_TIMEOUT(bFlacDecoded.load(std::memory_order_acquire), 5000);

        // (b) Prove the spectrum comes alive from B's decoded data.
        if (hasFftw) {
            QTRY_VERIFY_WITH_TIMEOUT(specNonZero(), 5000);
        } else {
            QTest::qWait(300);
        }
    }
};

QTEST_GUILESS_MAIN(TestVisualizationFeeder)
#include "test_visualization_feeder.moc"
