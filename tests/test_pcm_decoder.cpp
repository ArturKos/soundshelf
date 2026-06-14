#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "soundshelf/io/PcmDecoder.hpp"

using namespace soundshelf;

class TestPcmDecoder : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    /// Writes a canonical 44.1 kHz / stereo / 16-bit WAV with @p frames frames
    /// of a simple ramp, returning its path. Hermetic — no ffmpeg needed to
    /// create the input (only to decode it).
    QString writeWav(int frames) {
        const quint32 rate = 44100;
        const quint16 ch = 2, bits = 16;
        const quint32 dataBytes = quint32(frames) * ch * (bits / 8);
        const quint32 byteRate = rate * ch * (bits / 8);
        const quint16 blockAlign = ch * (bits / 8);

        QByteArray b;
        QDataStream s(&b, QIODevice::WriteOnly);
        s.setByteOrder(QDataStream::LittleEndian);
        b.append("RIFF");
        s.device()->seek(4); s << quint32(36 + dataBytes);
        b.append("WAVEfmt ");
        s.device()->seek(b.size());
        s << quint32(16) << quint16(1) << ch << rate << byteRate
          << blockAlign << bits;
        b.append("data");
        s.device()->seek(b.size());
        s << dataBytes;
        for (int i = 0; i < frames; ++i) {
            const qint16 v = qint16((i % 1000) * 30 - 15000);
            s << v << v;  // L, R
        }

        const QString path = m_dir.filePath(QStringLiteral("tone_%1.wav").arg(frames));
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(b);
        f.close();
        return path;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        if (!PcmDecoder::isAvailable())
            QSKIP("ffmpeg not on PATH — skipping PcmDecoder integration tests");
    }

    void decodesKnownFrameCount() {
        const int frames = 44100;  // exactly 1 second
        const QString wav = writeWav(frames);

        auto r = PcmDecoder::decodeToS16(wav, 44100, 2);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        const auto& buf = r.value();
        QCOMPARE(buf.sampleRate, 44100);
        QCOMPARE(buf.channels, 2);
        QCOMPARE(buf.totalSamples, frames);
        QCOMPARE(buf.s16le.size(), qsizetype(frames) * 2 * 2);
    }

    void streamYieldsSameTotal() {
        const int frames = 22050;  // half a second
        const QString wav = writeWav(frames);

        qint64 streamed = 0;
        int total = 0;
        auto r = PcmDecoder::streamS16(wav,
            [&streamed](const int16_t*, int f) { streamed += f; return true; },
            44100, 2, &total);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(total, frames);
        QCOMPARE(streamed, qint64(frames));
    }

    void sinkAbortIsReported() {
        const QString wav = writeWav(44100);
        auto r = PcmDecoder::streamS16(wav,
            [](const int16_t*, int) { return false; },  // abort immediately
            44100, 2, nullptr);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::OperationCancelled);
    }

    void missingFileFails() {
        auto r = PcmDecoder::decodeToS16(
            m_dir.filePath(QStringLiteral("nope.flac")), 44100, 2);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::FileNotFound);
    }

    void invalidParamsFail() {
        const QString wav = writeWav(100);
        auto r = PcmDecoder::decodeToS16(wav, 0, 2);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }
};

QTEST_GUILESS_MAIN(TestPcmDecoder)
#include "test_pcm_decoder.moc"
