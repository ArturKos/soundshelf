#include <QtTest>
#include <QVector>
#include <cmath>
#include "soundshelf/core/PlayerEngine.hpp"

using namespace soundshelf;

namespace {
QVector<float> sine(double freq, int n, int rate = 44100) {
    QVector<float> v(n);
    for (int i = 0; i < n; ++i)
        v[i] = 0.8f * float(std::sin(2.0 * M_PI * freq * i / rate));
    return v;
}
/// Index of the largest element.
int argmax(const QVector<float>& v) {
    int best = 0;
    for (int i = 1; i < v.size(); ++i) if (v[i] > v[best]) best = i;
    return best;
}
} // namespace

class TestSpectrum : public QObject {
    Q_OBJECT

private slots:

    void emptyInputGivesZeroBars() {
        const auto s = PlayerEngine::computeSpectrum({}, 24);
        QCOMPARE(s.size(), 24);
        for (float v : s) QCOMPARE(v, 0.0f);
    }

    void barsCountIsHonoured() {
        const auto s = PlayerEngine::computeSpectrum(sine(1000, 8192), 32);
        QCOMPARE(s.size(), 32);
    }

    void valuesAreNormalised() {
        const auto s = PlayerEngine::computeSpectrum(sine(1000, 8192), 24);
        for (float v : s) { QVERIFY(v >= 0.0f); QVERIFY(v <= 1.0f); }
    }

    void peakBandTracksFrequency() {
        if (PlayerEngine::computeSpectrum(sine(1000, 8192), 24).isEmpty())
            QSKIP("FFTW3 not compiled in");
        // A low tone should peak in a lower band than a high tone.
        const int lowBar  = argmax(PlayerEngine::computeSpectrum(sine(120,   8192), 24));
        const int highBar = argmax(PlayerEngine::computeSpectrum(sine(8000,  8192), 24));
        QVERIFY2(lowBar < highBar,
                 qPrintable(QStringLiteral("lowBar=%1 highBar=%2").arg(lowBar).arg(highBar)));
    }

    void pushPcmFeedsSpectrumData() {
        PlayerEngine pe;
        pe.pushVisualizationPcm(sine(2000, 8192));
        const auto s = pe.spectrumData(24);
        QCOMPARE(s.size(), 24);
        // With real PCM at least one band should be non-zero (when FFTW3 on).
        if (!PlayerEngine::computeSpectrum(sine(2000, 8192), 24).isEmpty()) {
            float maxv = 0; for (float v : s) maxv = std::max(maxv, v);
            QVERIFY(maxv > 0.0f);
        }
    }
};

QTEST_GUILESS_MAIN(TestSpectrum)
#include "test_spectrum.moc"
