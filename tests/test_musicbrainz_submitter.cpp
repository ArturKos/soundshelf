#include <QtTest>
#include <QMap>
#include <QUrlQuery>

#include "soundshelf/network/MusicBrainzSubmitter.hpp"

using namespace soundshelf;

class TestMusicBrainzSubmitter : public QObject {
    Q_OBJECT

private:
    // Build a QMap<key,value> from a fields list for convenient lookup.
    static QMap<QString, QString> toMap(const QList<QPair<QString, QString>>& fields)
    {
        QMap<QString, QString> m;
        for (const auto& [k, v] : fields)
            m.insert(k, v);
        return m;
    }

private slots:

    // (1) Single-artist disc with title/artist/year/barcode/label/catalogNo and 3 tracks
    void fullDisc_producesExpectedFields()
    {
        Disc disc;
        disc.title     = QStringLiteral("Oxygène");
        disc.artist    = QStringLiteral("Jean-Michel Jarre");
        disc.year      = 1976;
        disc.barcode   = QStringLiteral("1234567890123");
        disc.label     = QStringLiteral("Disques Motors");
        disc.catalogNo = QStringLiteral("DMX 001");
        disc.type      = DiscType::Physical;

        Track t1; t1.trackNumber = 1; t1.title = QStringLiteral("Oxygène (Part I)");
        t1.durationMs = 123456; t1.artist = disc.artist;
        Track t2; t2.trackNumber = 2; t2.title = QStringLiteral("Oxygène (Part II)");
        t2.durationMs = 234567; t2.artist = disc.artist;
        Track t3; t3.trackNumber = 3; t3.title = QStringLiteral("Oxygène (Part III)");
        t3.durationMs = 345678; t3.artist = disc.artist;
        disc.tracks = { t1, t2, t3 };

        const auto m = toMap(MusicBrainzSubmitter::buildSeedFields(disc));

        // Release-level fields
        QCOMPARE(m.value(QStringLiteral("name")), disc.title);
        QCOMPARE(m.value(QStringLiteral("artist_credit.names.0.name")), disc.artist);
        QCOMPARE(m.value(QStringLiteral("artist_credit.names.0.artist.name")), disc.artist);
        QCOMPARE(m.value(QStringLiteral("date.year")), QStringLiteral("1976"));
        QCOMPARE(m.value(QStringLiteral("barcode")), disc.barcode);
        QCOMPARE(m.value(QStringLiteral("labels.0.name")), disc.label);
        QCOMPARE(m.value(QStringLiteral("labels.0.catalog_number")), disc.catalogNo);
        QCOMPARE(m.value(QStringLiteral("mediums.0.format")), QStringLiteral("CD"));

        // Per-track fields
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.0.number")), QStringLiteral("1"));
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.0.name")), t1.title);
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.0.length")), QStringLiteral("123456"));

        QCOMPARE(m.value(QStringLiteral("mediums.0.track.1.number")), QStringLiteral("2"));
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.1.name")), t2.title);
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.1.length")), QStringLiteral("234567"));

        QCOMPARE(m.value(QStringLiteral("mediums.0.track.2.number")), QStringLiteral("3"));
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.2.name")), t3.title);
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.2.length")), QStringLiteral("345678"));

        // Same artist → no per-track artist credits
        QVERIFY(!m.contains(QStringLiteral("mediums.0.track.0.artist_credit.names.0.name")));
        QVERIFY(!m.contains(QStringLiteral("mediums.0.track.1.artist_credit.names.0.name")));
        QVERIFY(!m.contains(QStringLiteral("mediums.0.track.2.artist_credit.names.0.name")));
    }

    // (2) Empty optional fields (no barcode/label/year=0/durationMs=0) are omitted
    void emptyOptionalFields_omitted()
    {
        Disc disc;
        disc.title     = QStringLiteral("Unknown Album");
        disc.artist    = QStringLiteral("Unknown Artist");
        disc.year      = 0;
        disc.barcode   = QString{};
        disc.label     = QString{};
        disc.catalogNo = QString{};
        disc.type      = DiscType::Folder;

        Track t1; t1.title = QStringLiteral("Track 1"); t1.durationMs = 0;
        t1.artist = disc.artist;
        disc.tracks = { t1 };

        const auto m = toMap(MusicBrainzSubmitter::buildSeedFields(disc));

        QVERIFY(!m.contains(QStringLiteral("date.year")));
        QVERIFY(!m.contains(QStringLiteral("barcode")));
        QVERIFY(!m.contains(QStringLiteral("labels.0.name")));
        QVERIFY(!m.contains(QStringLiteral("labels.0.catalog_number")));
        QVERIFY(!m.contains(QStringLiteral("mediums.0.track.0.length")));
        QCOMPARE(m.value(QStringLiteral("mediums.0.format")), QStringLiteral("Digital Media"));
    }

    // (3) Various-artists disc: per-track artist_credit only for tracks differing from disc.artist
    void variousArtists_perTrackCreditOnlyForDifferent()
    {
        Disc disc;
        disc.title  = QStringLiteral("Now That's What I Call Music");
        disc.artist = QStringLiteral("Various Artists");
        disc.type   = DiscType::Physical;

        Track t1; t1.trackNumber = 1; t1.title = QStringLiteral("Bohemian Rhapsody");
        t1.artist = QStringLiteral("Queen");           // different
        Track t2; t2.trackNumber = 2; t2.title = QStringLiteral("Compilation Filler");
        t2.artist = QStringLiteral("Various Artists"); // same as disc
        Track t3; t3.trackNumber = 3; t3.title = QStringLiteral("Hotel California");
        t3.artist = QStringLiteral("Eagles");          // different
        disc.tracks = { t1, t2, t3 };

        const auto m = toMap(MusicBrainzSubmitter::buildSeedFields(disc));

        QCOMPARE(m.value(QStringLiteral("mediums.0.track.0.artist_credit.names.0.name")),
                 QStringLiteral("Queen"));

        QVERIFY(!m.contains(QStringLiteral("mediums.0.track.1.artist_credit.names.0.name")));

        QCOMPARE(m.value(QStringLiteral("mediums.0.track.2.artist_credit.names.0.name")),
                 QStringLiteral("Eagles"));
    }

    // (4) buildSeedUrl: correct scheme/host/path, query round-trips, special chars encoded
    void buildSeedUrl_correctStructureAndEncoding()
    {
        Disc disc;
        disc.title  = QStringLiteral("Oxygène");
        disc.artist = QStringLiteral("Jean-Michel Jarre & Friends");
        disc.year   = 1976;
        disc.type   = DiscType::Physical;

        Track t1; t1.trackNumber = 1; t1.title = QStringLiteral("Part I");
        t1.durationMs = 60000; t1.artist = disc.artist;
        disc.tracks = { t1 };

        const QUrl url = MusicBrainzSubmitter::buildSeedUrl(disc);

        QCOMPARE(url.scheme(), QStringLiteral("https"));
        QCOMPARE(url.host(), QStringLiteral("musicbrainz.org"));
        QCOMPARE(url.path(), QStringLiteral("/release/add"));
        QVERIFY(url.hasQuery());

        // Round-trip: decoded query items must match original values
        QUrlQuery q(url.query(QUrl::FullyEncoded));
        QMap<QString, QString> decoded;
        for (const auto& [k, v] : q.queryItems(QUrl::FullyDecoded))
            decoded.insert(k, v);

        QCOMPARE(decoded.value(QStringLiteral("name")), disc.title);
        QCOMPARE(decoded.value(QStringLiteral("artist_credit.names.0.name")), disc.artist);

        // The query string must contain percent-encoded special chars:
        //   é → %C3%A9 (UTF-8),  & in artist name → %26
        const QString qs = url.query(QUrl::FullyEncoded);
        const bool hasEncodedAccent =
            qs.contains(QLatin1String("%C3%A9"), Qt::CaseInsensitive);
        const bool hasEncodedAmpersand =
            qs.contains(QLatin1String("%26"), Qt::CaseInsensitive);
        QVERIFY(hasEncodedAccent || hasEncodedAmpersand);
    }

    // (5) Track number fallback: trackNumber==0 uses 1-based position
    void trackNumberFallback_usesOneBased()
    {
        Disc disc;
        disc.title  = QStringLiteral("Album");
        disc.artist = QStringLiteral("Artist");
        disc.type   = DiscType::Folder;

        Track t1; t1.trackNumber = 0; t1.title = QStringLiteral("First");  t1.artist = disc.artist;
        Track t2; t2.trackNumber = 0; t2.title = QStringLiteral("Second"); t2.artist = disc.artist;
        disc.tracks = { t1, t2 };

        const auto m = toMap(MusicBrainzSubmitter::buildSeedFields(disc));

        QCOMPARE(m.value(QStringLiteral("mediums.0.track.0.number")), QStringLiteral("1"));
        QCOMPARE(m.value(QStringLiteral("mediums.0.track.1.number")), QStringLiteral("2"));
    }

    // (6) Image disc type maps to "Digital Media"
    void imageDiscType_mapsToDigitalMedia()
    {
        Disc disc;
        disc.title  = QStringLiteral("Ripped Image");
        disc.artist = QStringLiteral("Artist");
        disc.type   = DiscType::Image;

        const auto m = toMap(MusicBrainzSubmitter::buildSeedFields(disc));

        QCOMPARE(m.value(QStringLiteral("mediums.0.format")), QStringLiteral("Digital Media"));
    }

    // (7) edit_note is appended when non-empty, omitted when empty
    void editNote_appendedOrOmitted()
    {
        Disc disc;
        disc.title  = QStringLiteral("Album");
        disc.artist = QStringLiteral("Artist");
        disc.type   = DiscType::Physical;

        const auto withNote = toMap(
            MusicBrainzSubmitter::buildSeedFields(disc, QStringLiteral("Seeded by SoundShelf")));
        QCOMPARE(withNote.value(QStringLiteral("edit_note")),
                 QStringLiteral("Seeded by SoundShelf"));

        const auto withoutNote = toMap(MusicBrainzSubmitter::buildSeedFields(disc));
        QVERIFY(!withoutNote.contains(QStringLiteral("edit_note")));
    }
};

QTEST_MAIN(TestMusicBrainzSubmitter)
#include "test_musicbrainz_submitter.moc"
