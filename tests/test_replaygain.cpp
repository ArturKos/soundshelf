#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <cmath>
#include "soundshelf/core/ReplayGainAnalyzer.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

using namespace soundshelf;

class TestReplayGain : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    /// Writes a 44.1 kHz stereo 16-bit WAV holding a 1 kHz sine of the given
    /// peak amplitude (in int16 units), 2 seconds long.
    QString writeSine(int amplitude) {
        const quint32 rate = 44100;
        const quint16 ch = 2, bits = 16;
        const int frames = rate * 2;
        const quint32 dataBytes = quint32(frames) * ch * (bits / 8);

        QByteArray b;
        QDataStream s(&b, QIODevice::WriteOnly);
        s.setByteOrder(QDataStream::LittleEndian);
        b.append("RIFF"); s.device()->seek(4); s << quint32(36 + dataBytes);
        b.append("WAVEfmt "); s.device()->seek(b.size());
        s << quint32(16) << quint16(1) << ch << rate
          << quint32(rate * ch * 2) << quint16(ch * 2) << bits;
        b.append("data"); s.device()->seek(b.size()); s << dataBytes;
        for (int i = 0; i < frames; ++i) {
            const double t = double(i) / rate;
            const qint16 v = qint16(amplitude * std::sin(2.0 * M_PI * 1000.0 * t));
            s << v << v;
        }
        const QString p = m_dir.filePath(QStringLiteral("sine_%1.wav").arg(amplitude));
        QFile f(p); f.open(QIODevice::WriteOnly); f.write(b); f.close();
        return p;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        if (!ReplayGainAnalyzer::isAvailable())
            QSKIP("libebur128 not compiled in");
        if (!PcmDecoder::isAvailable())
            QSKIP("ffmpeg not on PATH");
    }

    void availabilityIsTrueWhenCompiledIn() {
        QVERIFY(ReplayGainAnalyzer::isAvailable());
    }

    void louderSignalGetsLowerGain() {
        ReplayGainAnalyzer rg;
        // 20 dB amplitude difference (10000 vs 1000 int16 peak).
        auto loud  = rg.analyseFile(writeSine(10000));
        auto quiet = rg.analyseFile(writeSine(1000));
        QVERIFY2(loud.isOk(),  qPrintable(loud.isErr()  ? loud.error().message  : QString()));
        QVERIFY2(quiet.isOk(), qPrintable(quiet.isErr() ? quiet.error().message : QString()));

        // Quieter track needs ~20 dB more gain than the louder one.
        const double dGain = quiet.value().trackGainDb - loud.value().trackGainDb;
        QVERIFY2(std::abs(dGain - 20.0) < 1.0,
                 qPrintable(QStringLiteral("gain delta=%1 (expected ~20)").arg(dGain)));

        // LUFS difference mirrors the amplitude ratio.
        const double dLufs = loud.value().integratedLufs - quiet.value().integratedLufs;
        QVERIFY2(std::abs(dLufs - 20.0) < 1.0,
                 qPrintable(QStringLiteral("lufs delta=%1 (expected ~20)").arg(dLufs)));
    }

    void peakMatchesAmplitude() {
        ReplayGainAnalyzer rg;
        auto r = rg.analyseFile(writeSine(16384));  // -6 dBFS peak
        QVERIFY(r.isOk());
        // 16384/32768 = 0.5 linear.
        QVERIFY2(std::abs(r.value().trackPeak - 0.5) < 0.02,
                 qPrintable(QStringLiteral("peak=%1 (expected ~0.5)").arg(r.value().trackPeak)));
    }

    void albumGainAggregates() {
        ReplayGainAnalyzer rg;
        const QStringList paths{writeSine(10000), writeSine(1000)};
        auto r = rg.analyseAlbum(paths);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().tracks.size(), 2);
        // Album peak is the loudest track's peak.
        QVERIFY(r.value().albumPeak > 0.25);
        // Album gain sits between the two track gains.
        const double g0 = r.value().tracks[0].trackGainDb;
        const double g1 = r.value().tracks[1].trackGainDb;
        QVERIFY(r.value().albumGainDb <= std::max(g0, g1) + 0.01);
        QVERIFY(r.value().albumGainDb >= std::min(g0, g1) - 0.01);
    }

    void emptyAlbumFails() {
        ReplayGainAnalyzer rg;
        auto r = rg.analyseAlbum({});
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }
};

QTEST_GUILESS_MAIN(TestReplayGain)
#include "test_replaygain.moc"
