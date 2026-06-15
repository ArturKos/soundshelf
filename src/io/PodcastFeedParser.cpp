#include "soundshelf/io/PodcastFeedParser.hpp"

#include <QFile>
#include <QLoggingCategory>
#include <QXmlStreamReader>

Q_LOGGING_CATEGORY(lcPodcast, "soundshelf.podcast")

namespace soundshelf {
namespace {

// ---------------------------------------------------------------------------
// Element-level helpers (anonymous namespace — no external linkage)
// ---------------------------------------------------------------------------

/// Parse children of a standard RSS <image> or self-closing <itunes:image>.
/// Reader must be positioned AT the <image> start element on entry.
void parseChannelImage(QXmlStreamReader& r, QString& imageUrl) {
    const QString href = r.attributes().value(QLatin1String("href")).toString();
    if (!href.isEmpty()) {
        if (imageUrl.isEmpty())
            imageUrl = href;
        r.skipCurrentElement();
        return;
    }
    // Standard RSS <image><url>…</url>…</image>
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String("url")) {
            if (imageUrl.isEmpty())
                imageUrl = r.readElementText();
            else
                r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
}

/// Parse one <item> element into an Episode.
/// Reader must be positioned AT the <item> start element on entry.
PodcastFeedParser::Episode parseItem(QXmlStreamReader& r) {
    PodcastFeedParser::Episode ep;

    while (r.readNextStartElement()) {
        const auto ln = r.name();

        if (ln == QLatin1String("title")) {
            ep.title = r.readElementText();

        } else if (ln == QLatin1String("description")) {
            if (ep.description.isEmpty())
                ep.description = r.readElementText();
            else
                r.skipCurrentElement();

        } else if (ln == QLatin1String("summary")) {
            // itunes:summary — fallback description
            if (ep.description.isEmpty())
                ep.description = r.readElementText();
            else
                r.skipCurrentElement();

        } else if (ln == QLatin1String("guid")) {
            ep.guid = r.readElementText();

        } else if (ln == QLatin1String("pubDate")) {
            ep.pubDate = QDateTime::fromString(r.readElementText(), Qt::RFC2822Date);

        } else if (ln == QLatin1String("enclosure")) {
            ep.enclosureUrl  = r.attributes().value(QLatin1String("url")).toString();
            ep.enclosureType = r.attributes().value(QLatin1String("type")).toString();
            const QString lenStr = r.attributes().value(QLatin1String("length")).toString();
            if (!lenStr.isEmpty()) {
                bool ok = false;
                const qint64 len = lenStr.toLongLong(&ok);
                if (ok) ep.enclosureLength = len;
            }
            r.skipCurrentElement();

        } else if (ln == QLatin1String("duration")) {
            // itunes:duration
            ep.durationMs = PodcastFeedParser::parseItunesDuration(r.readElementText());

        } else if (ln == QLatin1String("episode")) {
            // itunes:episode
            bool ok = false;
            const int num = r.readElementText().toInt(&ok);
            if (ok) ep.episodeNumber = num;

        } else {
            r.skipCurrentElement();
        }
    }

    // guid fallback: use enclosureUrl when <guid> is absent
    if (ep.guid.isEmpty())
        ep.guid = ep.enclosureUrl;

    return ep;
}

/// Parse one <channel> element into a Feed.
/// Reader must be positioned AT the <channel> start element on entry.
PodcastFeedParser::Feed parseChannel(QXmlStreamReader& r) {
    PodcastFeedParser::Feed feed;

    while (r.readNextStartElement()) {
        const auto ln = r.name();

        if (ln == QLatin1String("title")) {
            feed.title = r.readElementText();

        } else if (ln == QLatin1String("link")) {
            feed.link = r.readElementText();

        } else if (ln == QLatin1String("description")) {
            if (feed.description.isEmpty())
                feed.description = r.readElementText();
            else
                r.skipCurrentElement();

        } else if (ln == QLatin1String("language")) {
            feed.language = r.readElementText();

        } else if (ln == QLatin1String("author")) {
            // itunes:author (local name "author" regardless of prefix)
            if (feed.author.isEmpty())
                feed.author = r.readElementText();
            else
                r.skipCurrentElement();

        } else if (ln == QLatin1String("summary")) {
            // itunes:summary — fallback channel description
            if (feed.description.isEmpty())
                feed.description = r.readElementText();
            else
                r.skipCurrentElement();

        } else if (ln == QLatin1String("image")) {
            parseChannelImage(r, feed.imageUrl);

        } else if (ln == QLatin1String("item")) {
            feed.episodes.append(parseItem(r));

        } else {
            r.skipCurrentElement();
        }
    }

    return feed;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// PodcastFeedParser — public methods
// ---------------------------------------------------------------------------

PodcastFeedParser::PodcastFeedParser(QObject* parent) : QObject(parent) {}
PodcastFeedParser::~PodcastFeedParser() = default;

int PodcastFeedParser::parseItunesDuration(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty()) return 0;

    const QStringList parts = t.split(QLatin1Char(':'));
    bool ok = false;

    if (parts.size() == 1) {
        const int secs = parts[0].toInt(&ok);
        return ok ? secs * 1000 : 0;
    }

    if (parts.size() == 2) {
        const int mm = parts[0].toInt(&ok);
        if (!ok) return 0;
        const int ss = parts[1].toInt(&ok);
        return ok ? (mm * 60 + ss) * 1000 : 0;
    }

    // HH:MM:SS (or more components — use first three)
    const int hh = parts[0].toInt(&ok);
    if (!ok) return 0;
    const int mm = parts[1].toInt(&ok);
    if (!ok) return 0;
    const int ss = parts[2].toInt(&ok);
    return ok ? (hh * 3600 + mm * 60 + ss) * 1000 : 0;
}

Result<PodcastFeedParser::Feed> PodcastFeedParser::parseFile(const QString& path) {
    QFile f(path);
    if (!f.exists()) {
        return Result<Feed>::err(Error::FileNotFound,
            tr("Podcast feed file not found: %1").arg(path));
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return Result<Feed>::err(Error::FileAccessDenied,
            tr("Cannot open podcast feed file: %1").arg(path));
    }
    return parseBytes(f.readAll(), path);
}

Result<PodcastFeedParser::Feed> PodcastFeedParser::parseBytes(const QByteArray& xml,
                                                               const QString& sourceLabel) {
    const QString label = sourceLabel.isEmpty() ? QStringLiteral("(input)") : sourceLabel;
    QXmlStreamReader r(xml);
    Feed feed;
    bool hasChannel = false;

    // Traverse the document looking for <rss><channel> (or <channel> as root).
    // XML has one root element: break after processing it to avoid calling
    // readNext() past </rss>, which would trigger "Premature end of document".
    while (r.readNextStartElement()) {
        const auto ln = r.name();
        if (ln == QLatin1String("rss") || ln == QLatin1String("rdf:RDF")) {
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("channel")) {
                    hasChannel = true;
                    feed = parseChannel(r);
                } else {
                    r.skipCurrentElement();
                }
            }
            break;
        } else if (ln == QLatin1String("channel")) {
            // Permissive: accept <channel> as the document root.
            hasChannel = true;
            feed = parseChannel(r);
            break;
        } else {
            r.skipCurrentElement();
        }
    }

    if (r.hasError()) {
        return Result<Feed>::err(Error::InvalidFormat,
            tr("XML parse error in '%1': %2").arg(label, r.errorString()));
    }
    if (!hasChannel) {
        return Result<Feed>::err(Error::InvalidFormat,
            tr("No <channel> element found in '%1'").arg(label));
    }

    qCDebug(lcPodcast) << "Parsed feed" << label
                       << "title:" << feed.title
                       << "episodes:" << feed.episodes.size();
    return Result<Feed>::ok(std::move(feed));
}

} // namespace soundshelf
