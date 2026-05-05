#include <QtTest>
#include "soundshelf/core/DuplicateDetector.hpp"

using namespace soundshelf;

namespace {

Track makeTrack(int id, const QString& title, const QString& artist,
                const QString& album, int durationMs,
                const QString& acoustid = {}) {
    Track t;
    t.id = id;
    t.title = title;
    t.artist = artist;
    t.album = album;
    t.durationMs = durationMs;
    t.acoustid = acoustid;
    t.filepath = QStringLiteral("/tmp/%1.mp3").arg(id);
    return t;
}

} // namespace

class TestDuplicateDetector : public QObject {
    Q_OBJECT

private slots:

    void groupByTagsClustersIdenticalTracks() {
        // Three "Smells Like Teen Spirit" copies, slightly different
        // durations within the 2 s bucket; one totally different track.
        const QList<Track> tracks {
            makeTrack(1, QStringLiteral("Smells Like Teen Spirit"),
                      QStringLiteral("Nirvana"), QStringLiteral("Nevermind"), 301000),
            makeTrack(2, QStringLiteral("Smells Like Teen Spirit"),
                      QStringLiteral("Nirvana"), QStringLiteral("Nevermind"), 301500),
            makeTrack(3, QStringLiteral("Smells Like Teen Spirit"),
                      QStringLiteral("Nirvana"), QStringLiteral("Nevermind"), 302100),
            makeTrack(4, QStringLiteral("Lithium"),
                      QStringLiteral("Nirvana"), QStringLiteral("Nevermind"), 257000),
        };

        const auto groups = DuplicateDetector::groupByTags(tracks);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.first().tracks.size(), 3);
        QCOMPARE(groups.first().reason, DuplicateDetector::ByTags);
    }

    void groupByTagsIsCaseInsensitive() {
        const QList<Track> tracks {
            makeTrack(1, QStringLiteral("Hey Joe"), QStringLiteral("HENDRIX"),
                      QStringLiteral("Are You Experienced"), 210000),
            makeTrack(2, QStringLiteral("hey joe"), QStringLiteral("Hendrix"),
                      QStringLiteral("are you experienced"), 210500),
        };
        const auto groups = DuplicateDetector::groupByTags(tracks);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.first().tracks.size(), 2);
    }

    void groupByTagsBucketsBy2sDuration() {
        // 60s and 63s should NOT collide (>2s apart).
        const QList<Track> tracks {
            makeTrack(1, QStringLiteral("X"), QStringLiteral("A"),
                      QStringLiteral("B"), 60000),
            makeTrack(2, QStringLiteral("X"), QStringLiteral("A"),
                      QStringLiteral("B"), 63000),
        };
        const auto groups = DuplicateDetector::groupByTags(tracks);
        QCOMPARE(groups.size(), 0);
    }

    void groupByTagsIgnoresTracksWithoutTitle() {
        const QList<Track> tracks {
            makeTrack(1, QString(),
                      QStringLiteral("A"), QStringLiteral("B"), 60000),
            makeTrack(2, QString(),
                      QStringLiteral("A"), QStringLiteral("B"), 60500),
        };
        const auto groups = DuplicateDetector::groupByTags(tracks);
        QCOMPARE(groups.size(), 0);
    }

    void groupByAcoustIdMatchesById() {
        const QList<Track> tracks {
            makeTrack(1, QStringLiteral("a"), QStringLiteral("x"),
                      QStringLiteral(""), 100000,
                      QStringLiteral("aaaa-bbbb-cccc")),
            makeTrack(2, QStringLiteral("totally different"),
                      QStringLiteral("y"), QStringLiteral(""), 200000,
                      QStringLiteral("aaaa-bbbb-cccc")),
            makeTrack(3, QStringLiteral("a"), QStringLiteral("x"),
                      QStringLiteral(""), 100000,
                      QStringLiteral("zzzz")),
        };
        const auto groups = DuplicateDetector::groupByAcoustId(tracks);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.first().tracks.size(), 2);
        QCOMPARE(groups.first().reason, DuplicateDetector::ByAcoustId);
    }

    void groupByAcoustIdSkipsEmptyFingerprints() {
        const QList<Track> tracks {
            makeTrack(1, QStringLiteral("a"), QStringLiteral("x"),
                      QStringLiteral(""), 100000),
            makeTrack(2, QStringLiteral("a"), QStringLiteral("x"),
                      QStringLiteral(""), 100000),
        };
        const auto groups = DuplicateDetector::groupByAcoustId(tracks);
        QCOMPARE(groups.size(), 0);
    }
};

QTEST_APPLESS_MAIN(TestDuplicateDetector)
#include "test_duplicate_detector.moc"
