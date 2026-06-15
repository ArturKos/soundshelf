#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Parser for podcast RSS 2.0 feeds (with iTunes namespace extensions).
 *
 * Converts in-memory XML bytes or a local file into a structured @ref Feed
 * and @ref Episode list. Performs **no** network I/O — fetching is handled
 * by a higher-level manager. This is a pure I/O-layer component, analogous
 * to @c io::CueParser.
 *
 * Supported standard elements: channel @c title / @c description / @c link /
 * @c language / @c image, item @c title / @c description / @c guid / @c pubDate /
 * @c enclosure.
 *
 * Supported iTunes extensions (@c xmlns:itunes):
 * @c itunes:author, @c itunes:image, @c itunes:summary, @c itunes:duration,
 * @c itunes:episode. Namespace prefix variations are handled by matching local
 * element names, so alternative prefixes work transparently.
 *
 * Unknown elements are silently skipped. A feed with zero @c \<item\> elements
 * is valid — it returns an @ref Ok result with an empty episodes list.
 */
class PodcastFeedParser : public QObject {
    Q_OBJECT
public:
    /**
     * @brief A single podcast episode parsed from an @c \<item\> element.
     */
    struct Episode {
        QString guid;               ///< \<guid\> or @c enclosureUrl fallback when absent
        QString title;              ///< \<title\>
        QString description;        ///< \<description\> or \<itunes:summary\>
        QString enclosureUrl;       ///< \<enclosure url=…\>
        QString enclosureType;      ///< \<enclosure type=…\>, e.g. @c "audio/mpeg"
        qint64  enclosureLength = 0; ///< \<enclosure length=…\> in bytes; 0 if absent
        QDateTime pubDate;          ///< \<pubDate\> parsed from RFC 822
        int durationMs = 0;         ///< \<itunes:duration\> converted to milliseconds
        int episodeNumber = 0;      ///< \<itunes:episode\>; 0 if absent
    };

    /**
     * @brief A parsed podcast feed (channel).
     */
    struct Feed {
        QString title;              ///< Channel \<title\>
        QString author;             ///< \<itunes:author\> or channel-level fallback
        QString description;        ///< Channel \<description\> / \<itunes:summary\>
        QString imageUrl;           ///< \<itunes:image href\> or \<image\>\<url\>
        QString link;               ///< Channel \<link\>
        QString language;           ///< \<language\>
        QList<Episode> episodes;    ///< Episodes in document order (newest-first typical)
    };

    /// Constructs the parser.
    explicit PodcastFeedParser(QObject* parent = nullptr);
    ~PodcastFeedParser() override;

    /**
     * @brief Reads and parses an RSS 2.0 feed from a local file.
     *
     * @param path  Absolute or relative path to the .xml / .rss file.
     * @return Parsed @ref Feed on success, or an @ref Error on file-open or
     *         parse failure.
     */
    Result<Feed> parseFile(const QString& path);

    /**
     * @brief Parses an RSS 2.0 feed from an in-memory byte array.
     *
     * This is the core entry point and the method used by unit tests.
     *
     * @param xml          Raw bytes (UTF-8 or encoding declared in the XML header).
     * @param sourceLabel  Optional label used in error messages and debug output
     *                     (e.g. a URL or filename).
     * @return Parsed @ref Feed on success, or an @ref Error if the XML is
     *         malformed or no @c \<channel\> element is present.
     */
    Result<Feed> parseBytes(const QByteArray& xml, const QString& sourceLabel = {});

    /**
     * @brief Converts an @c \<itunes:duration\> string to milliseconds.
     *
     * Accepts three formats:
     *  - @c "HH:MM:SS" — hours, minutes, seconds
     *  - @c "MM:SS"    — minutes, seconds
     *  - @c "NNN"      — plain integer seconds
     *
     * @param text  The raw duration string from the feed.
     * @return Duration in milliseconds, or 0 for empty or unparseable input.
     */
    static int parseItunesDuration(const QString& text);
};

} // namespace soundshelf
