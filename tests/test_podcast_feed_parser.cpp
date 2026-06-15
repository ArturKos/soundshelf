#include <QtTest>
#include "soundshelf/io/PodcastFeedParser.hpp"

using namespace soundshelf;

// ---------------------------------------------------------------------------
// XML fixtures — kept outside the class body so Qt 6.4 moc can tokenise the
// file correctly (raw string literals containing '"' confuse the moc lexer).
// ---------------------------------------------------------------------------
namespace {

QByteArray xmlTwoEpisodes() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<rss version=\"2.0\" xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\">\n"
           "  <channel>\n"
           "    <title>Test Podcast</title>\n"
           "    <link>https://example.com/podcast</link>\n"
           "    <description>A test podcast feed</description>\n"
           "    <language>en-US</language>\n"
           "    <itunes:author>Jane Doe</itunes:author>\n"
           "    <itunes:image href=\"https://example.com/cover.jpg\"/>\n"
           "    <item>\n"
           "      <title>Episode 1: The Beginning</title>\n"
           "      <guid>https://example.com/ep1</guid>\n"
           "      <pubDate>Mon, 01 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep1.mp3\""
           " type=\"audio/mpeg\" length=\"12345678\"/>\n"
           "      <itunes:duration>01:02:03</itunes:duration>\n"
           "      <itunes:episode>1</itunes:episode>\n"
           "      <description>First episode description.</description>\n"
           "    </item>\n"
           "    <item>\n"
           "      <title>Episode 2: The Return</title>\n"
           "      <guid>https://example.com/ep2</guid>\n"
           "      <pubDate>Mon, 08 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep2.mp3\""
           " type=\"audio/mpeg\" length=\"98765432\"/>\n"
           "      <itunes:duration>00:45:30</itunes:duration>\n"
           "      <itunes:episode>2</itunes:episode>\n"
           "    </item>\n"
           "  </channel>\n"
           "</rss>";
}

QByteArray xmlNoGuid() {
    return "<?xml version=\"1.0\"?>\n"
           "<rss version=\"2.0\">\n"
           "  <channel>\n"
           "    <title>No-Guid Podcast</title>\n"
           "    <item>\n"
           "      <title>Episode Without Guid</title>\n"
           "      <enclosure url=\"https://example.com/noguid.mp3\""
           " type=\"audio/mpeg\" length=\"0\"/>\n"
           "    </item>\n"
           "  </channel>\n"
           "</rss>";
}

QByteArray xmlEmptyChannel() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<rss version=\"2.0\">\n"
           "  <channel>\n"
           "    <title>Empty Podcast</title>\n"
           "    <description>No episodes yet.</description>\n"
           "  </channel>\n"
           "</rss>";
}

QByteArray xmlStandardImage() {
    return "<?xml version=\"1.0\"?>\n"
           "<rss version=\"2.0\">\n"
           "  <channel>\n"
           "    <title>Image Test</title>\n"
           "    <image>\n"
           "      <url>https://example.com/standard-cover.png</url>\n"
           "      <title>Image Test</title>\n"
           "      <link>https://example.com</link>\n"
           "    </image>\n"
           "  </channel>\n"
           "</rss>";
}

QByteArray xmlItunesSummary() {
    return "<?xml version=\"1.0\"?>\n"
           "<rss version=\"2.0\" xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\">\n"
           "  <channel>\n"
           "    <title>Summary Test</title>\n"
           "    <itunes:summary>Channel summary via iTunes.</itunes:summary>\n"
           "    <item>\n"
           "      <title>Ep</title>\n"
           "      <itunes:summary>Episode summary via iTunes.</itunes:summary>\n"
           "      <enclosure url=\"https://example.com/ep.mp3\""
           " type=\"audio/mpeg\" length=\"1\"/>\n"
           "    </item>\n"
           "  </channel>\n"
           "</rss>";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestPodcastFeedParser : public QObject {
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // (1) Well-formed RSS 2.0 + iTunes feed with 2 episodes
    // -----------------------------------------------------------------------
    void wellFormedFeedWithTwoEpisodes() {
        PodcastFeedParser parser;
        auto r = parser.parseBytes(xmlTwoEpisodes(), QStringLiteral("test-feed"));
        QVERIFY(r.isOk());
        const auto& feed = r.value();

        QCOMPARE(feed.title,    QStringLiteral("Test Podcast"));
        QCOMPARE(feed.author,   QStringLiteral("Jane Doe"));
        QCOMPARE(feed.imageUrl, QStringLiteral("https://example.com/cover.jpg"));
        QCOMPARE(feed.link,     QStringLiteral("https://example.com/podcast"));
        QCOMPARE(feed.language, QStringLiteral("en-US"));
        QCOMPARE(feed.episodes.size(), 2);

        // Episode 1
        const auto& ep1 = feed.episodes[0];
        QCOMPARE(ep1.guid,            QStringLiteral("https://example.com/ep1"));
        QCOMPARE(ep1.title,           QStringLiteral("Episode 1: The Beginning"));
        QCOMPARE(ep1.enclosureUrl,    QStringLiteral("https://example.com/ep1.mp3"));
        QCOMPARE(ep1.enclosureType,   QStringLiteral("audio/mpeg"));
        QCOMPARE(ep1.enclosureLength, qint64(12345678));
        QVERIFY(ep1.pubDate.isValid());
        QCOMPARE(ep1.durationMs,      3723000); // 01:02:03 = 3723 s
        QCOMPARE(ep1.episodeNumber,   1);
        QCOMPARE(ep1.description,     QStringLiteral("First episode description."));

        // Episode 2
        const auto& ep2 = feed.episodes[1];
        QCOMPARE(ep2.guid,            QStringLiteral("https://example.com/ep2"));
        QCOMPARE(ep2.enclosureUrl,    QStringLiteral("https://example.com/ep2.mp3"));
        QCOMPARE(ep2.enclosureLength, qint64(98765432));
        QCOMPARE(ep2.durationMs,      2730000); // 00:45:30 = 2730 s
        QCOMPARE(ep2.episodeNumber,   2);
    }

    // -----------------------------------------------------------------------
    // (2) parseItunesDuration — three formats + edge cases
    // -----------------------------------------------------------------------
    void parseItunesDurationFormats() {
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("01:02:03")), 3723000);
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("45:30")),    2730000);
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("512")),      512000);
        // Edge cases
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("")),         0);
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("abc")),      0);
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("0")),        0);
        QCOMPARE(PodcastFeedParser::parseItunesDuration(QStringLiteral("00:00:00")), 0);
    }

    // -----------------------------------------------------------------------
    // (3) guid fallback to enclosure url when <guid> is absent
    // -----------------------------------------------------------------------
    void guidFallbackToEnclosureUrl() {
        PodcastFeedParser parser;
        auto r = parser.parseBytes(xmlNoGuid(), QStringLiteral("guid-test"));
        QVERIFY(r.isOk());
        QCOMPARE(r.value().episodes.size(), 1);
        QCOMPARE(r.value().episodes[0].guid,
                 QStringLiteral("https://example.com/noguid.mp3"));
    }

    // -----------------------------------------------------------------------
    // (4) Empty channel (zero items) => Ok with empty episodes list
    // -----------------------------------------------------------------------
    void emptyChannelIsValid() {
        PodcastFeedParser parser;
        auto r = parser.parseBytes(xmlEmptyChannel(), QStringLiteral("empty-feed"));
        QVERIFY(r.isOk());
        const auto& feed = r.value();
        QCOMPARE(feed.title, QStringLiteral("Empty Podcast"));
        QVERIFY(feed.episodes.isEmpty());
    }

    // -----------------------------------------------------------------------
    // (5) Malformed / non-XML bytes => isErr() true
    // -----------------------------------------------------------------------
    void malformedXmlReturnsError() {
        const QByteArray xml = "<unclosed-element";
        PodcastFeedParser parser;
        auto r = parser.parseBytes(xml, QStringLiteral("bad-input"));
        QVERIFY(r.isErr());
    }

    // -----------------------------------------------------------------------
    // Extra: standard RSS <image><url> fallback
    // -----------------------------------------------------------------------
    void standardRssImageUrl() {
        PodcastFeedParser parser;
        auto r = parser.parseBytes(xmlStandardImage());
        QVERIFY(r.isOk());
        QCOMPARE(r.value().imageUrl,
                 QStringLiteral("https://example.com/standard-cover.png"));
    }

    // -----------------------------------------------------------------------
    // Extra: itunes:summary used as description fallback
    // -----------------------------------------------------------------------
    void itunesSummaryFallback() {
        PodcastFeedParser parser;
        auto r = parser.parseBytes(xmlItunesSummary());
        QVERIFY(r.isOk());
        QCOMPARE(r.value().description,
                 QStringLiteral("Channel summary via iTunes."));
        QCOMPARE(r.value().episodes[0].description,
                 QStringLiteral("Episode summary via iTunes."));
    }
};

QTEST_APPLESS_MAIN(TestPodcastFeedParser)
#include "test_podcast_feed_parser.moc"
