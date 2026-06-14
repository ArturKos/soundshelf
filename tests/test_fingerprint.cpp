#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <cmath>
#include "soundshelf/core/ChromaprintEngine.hpp"
#include "soundshelf/io/PcmDecoder.hpp"

using namespace soundshelf;

class TestFingerprint : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    /// 44.1 kHz stereo 16-bit WAV, @p seconds of a 440 Hz sine.
    QString writeSine(int seconds) {
        const quint32 rate = 44100;
        const quint16 ch = 2, bits = 16;
        const int frames = rate * seconds;
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
            const qint16 v = qint16(12000 * std::sin(2.0 * M_PI * 440.0 * i / rate));
            s << v << v;
        }
        const QString p = m_dir.filePath(QStringLiteral("sine_%1.wav").arg(seconds));
        QFile f(p); f.open(QIODevice::WriteOnly); f.write(b); f.close();
        return p;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        if (!ChromaprintEngine::isAvailable())
            QSKIP("libchromaprint not compiled in");
        if (!PcmDecoder::isAvailable())
            QSKIP("ffmpeg not on PATH");
    }

    void availabilityIsTrueWhenCompiledIn() {
        QVERIFY(ChromaprintEngine::isAvailable());
    }

    void fingerprintFileProducesResult() {
        ChromaprintEngine cp;
        auto r = cp.fingerprintFile(writeSine(10));
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QVERIFY(!r.value().fingerprint.isEmpty());
        QCOMPARE(r.value().durationSec, 10);
    }

    void fingerprintIsDeterministic() {
        ChromaprintEngine cp;
        const QString wav = writeSine(8);
        auto a = cp.fingerprintFile(wav);
        auto b = cp.fingerprintFile(wav);
        QVERIFY(a.isOk() && b.isOk());
        QCOMPARE(a.value().fingerprint, b.value().fingerprint);
    }

    void missingFileFails() {
        ChromaprintEngine cp;
        auto r = cp.fingerprintFile(m_dir.filePath(QStringLiteral("nope.flac")));
        QVERIFY(r.isErr());
    }

    void pcmRejectsBadParams() {
        ChromaprintEngine cp;
        auto r = cp.fingerprintPcm(QByteArray(1000, '\0'), 0, 2, 250);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }
};

QTEST_GUILESS_MAIN(TestFingerprint)
#include "test_fingerprint.moc"
