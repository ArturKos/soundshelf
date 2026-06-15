#include <QtTest>
#include "soundshelf/network/AccurateRip.hpp"
#include "soundshelf/io/DiscReader.hpp"

using namespace soundshelf;
using namespace soundshelf::accuraterip;

namespace {

/// Build a minimal Toc from a list of (startSector, endSector) pairs.
Toc makeToc(const QList<QPair<long, long>>& tracks) {
    Toc toc;
    int n = 1;
    for (const auto& [start, end] : tracks) {
        TocEntry e;
        e.trackNumber  = n++;
        e.startSector  = start;
        e.endSector    = end;
        toc.entries.append(e);
    }
    return toc;
}

/// Append a 32-bit little-endian value to @p ba.
void appendU32(QByteArray& ba, quint32 v) {
    ba.append(char(v & 0xFF));
    ba.append(char((v >>  8) & 0xFF));
    ba.append(char((v >> 16) & 0xFF));
    ba.append(char((v >> 24) & 0xFF));
}

/// Build a single dBAR chunk with two tracks.
QByteArray buildSinglePressingBlob(
    quint32 id1, quint32 id2, quint32 freedbId,
    quint8 conf0, quint32 crc0, quint32 crc4500,
    quint8 conf1, quint32 crc1, quint32 crc4501)
{
    QByteArray blob;
    blob.append(char(2));  // trackCount = 2
    appendU32(blob, id1);
    appendU32(blob, id2);
    appendU32(blob, freedbId);
    // track 0
    blob.append(char(conf0));
    appendU32(blob, crc0);
    appendU32(blob, crc4500);
    // track 1
    blob.append(char(conf1));
    appendU32(blob, crc1);
    appendU32(blob, crc4501);
    return blob;
}

} // namespace

class TestAccurateRip : public QObject {
    Q_OBJECT

private slots:

    // ── computeDiscIds ──────────────────────────────────────────────────

    void computeIds_twoTracks() {
        // track1 startSector=0, track2 startSector=10000, last endSector=19999
        // leadout = 20000
        const Toc toc = makeToc({{0, 9999}, {10000, 19999}});

        const DiscIds ids = computeDiscIds(toc);

        QCOMPARE(ids.trackCount, 2);

        // id1 = 0 + 10000 + 20000 = 30000
        QCOMPARE(ids.id1, quint32(30000));

        // id2: i=1 offset=0→use 1: 1*1=1
        //      i=2 offset=10000:    10000*2=20000
        //      leadout*3:           20000*3=60000
        //      total = 80001
        QCOMPARE(ids.id2, quint32(80001));

        // CDDB:
        //   track1: (0+150)/75 = 2s   → digit_sum(2)=2
        //   track2: (10000+150)/75 = 135s → digit_sum(135)=1+3+5=9
        //   n = 11
        //   leadoutSec = (20000+150)/75 = 268, track1Sec = 2, t = 266
        //   freedbId = (11<<24)|(266<<8)|2 = 0x0B010A02
        QCOMPARE(ids.freedbId, quint32(0x0B010A02u));
    }

    void computeIds_emptyToc() {
        const Toc toc;
        const DiscIds ids = computeDiscIds(toc);
        QCOMPARE(ids.trackCount, 0);
        QCOMPARE(ids.id1, quint32(0));
        QCOMPARE(ids.id2, quint32(0));
        QCOMPARE(ids.freedbId, quint32(0));
    }

    // ── parseResponse ───────────────────────────────────────────────────

    void parseResponse_emptyBlob() {
        auto r = parseResponse(QByteArray{});
        QVERIFY(r.isOk());
        QVERIFY(r.value().isEmpty());
    }

    void parseResponse_singlePressing() {
        const QByteArray blob = buildSinglePressingBlob(
            30000, 80001, 0x0B010A02u,
            /*track0*/ 3, 0x12345678u, 0xABCDEF01u,
            /*track1*/ 5, 0x87654321u, 0xFEDCBA98u);

        auto r = parseResponse(blob);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        const auto& results = r.value();
        QCOMPARE(results.size(), 1);

        const PressingResult& pr = results[0];
        QCOMPARE(pr.ids.trackCount, 2);
        QCOMPARE(pr.ids.id1,      quint32(30000));
        QCOMPARE(pr.ids.id2,      quint32(80001));
        QCOMPARE(pr.ids.freedbId, quint32(0x0B010A02u));

        QCOMPARE(pr.tracks.size(), 2);
        QCOMPARE(pr.tracks[0].confidence, quint8(3));
        QCOMPARE(pr.tracks[0].crc,        quint32(0x12345678u));
        QCOMPARE(pr.tracks[0].crc450,     quint32(0xABCDEF01u));
        QCOMPARE(pr.tracks[1].confidence, quint8(5));
        QCOMPARE(pr.tracks[1].crc,        quint32(0x87654321u));
        QCOMPARE(pr.tracks[1].crc450,     quint32(0xFEDCBA98u));
    }

    void parseResponse_twoPressings() {
        QByteArray blob;
        blob += buildSinglePressingBlob(1, 2, 3, 1, 0xAAAAu, 0xBBBBu, 2, 0xCCCCu, 0xDDDDu);
        blob += buildSinglePressingBlob(4, 5, 6, 7, 0xEEEEu, 0xFFFFu, 8, 0x1111u, 0x2222u);

        auto r = parseResponse(blob);
        QVERIFY(r.isOk());
        QCOMPARE(r.value().size(), 2);
        QCOMPARE(r.value()[0].ids.id1, quint32(1));
        QCOMPARE(r.value()[1].ids.id1, quint32(4));
    }

    void parseResponse_truncatedHeader() {
        // Only 5 bytes — too short for a 13-byte header
        QByteArray blob(5, '\x00');
        auto r = parseResponse(blob);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidFormat);
    }

    void parseResponse_truncatedTrackEntries() {
        // Valid 13-byte header for TC=2 but zero track bytes follow
        QByteArray blob;
        blob.append(char(2));  // tc=2
        appendU32(blob, 1);    // id1
        appendU32(blob, 2);    // id2
        appendU32(blob, 3);    // freedbId
        // deliberately omit the 2×9=18 track bytes
        auto r = parseResponse(blob);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidFormat);
    }
};

QTEST_GUILESS_MAIN(TestAccurateRip)
#include "test_accuraterip.moc"
