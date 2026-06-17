#include <QtTest>
#include <QVector>
#include <cmath>

#include "soundshelf/core/VisualizationFeeder.hpp"
#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

using namespace soundshelf;

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
};

QTEST_GUILESS_MAIN(TestVisualizationFeeder)
#include "test_visualization_feeder.moc"
