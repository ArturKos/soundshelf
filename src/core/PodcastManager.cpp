#include "soundshelf/core/PodcastManager.hpp"
#include "soundshelf/io/PodcastFeedParser.hpp"
#include "soundshelf/data/PodcastStore.hpp"
#include "soundshelf/network/RestClient.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcPodcastMgr, "soundshelf.podcast.manager")

namespace soundshelf {

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------

namespace {

QString mimeToExtension(const QString& mime) {
    if (mime == QLatin1String("audio/mpeg"))    return QStringLiteral(".mp3");
    if (mime == QLatin1String("audio/ogg"))     return QStringLiteral(".ogg");
    if (mime == QLatin1String("audio/mp4"))     return QStringLiteral(".m4a");
    if (mime == QLatin1String("audio/x-m4a"))   return QStringLiteral(".m4a");
    if (mime == QLatin1String("audio/aac"))     return QStringLiteral(".aac");
    if (mime == QLatin1String("audio/flac"))    return QStringLiteral(".flac");
    if (mime == QLatin1String("audio/wav"))     return QStringLiteral(".wav");
    if (mime == QLatin1String("audio/x-wav"))   return QStringLiteral(".wav");
    if (mime == QLatin1String("audio/opus"))    return QStringLiteral(".opus");
    if (mime == QLatin1String("audio/webm"))    return QStringLiteral(".webm");
    return {};
}

/// Converts a raw title or GUID into a filesystem-safe name (no path separators,
/// no shell-special chars). Maximum 180 characters.
QString safeFilename(const QString& title, const QString& guid) {
    QString name = title.isEmpty() ? guid : title;
    static const QRegularExpression kUnsafe(QStringLiteral("[^\\w\\s-]"));
    name.replace(kUnsafe, QStringLiteral("_"));
    name = name.simplified().replace(QLatin1Char(' '), QLatin1Char('_'));
    if (name.isEmpty())
        name = QStringLiteral("episode");
    return name.left(180);
}

/// Splits @p url into (scheme+host+port, path, query) and calls
/// RestClient::getBytes(), blocking on the returned QFuture. Callers must
/// invoke from a worker thread — blocking on the GUI thread deadlocks.
Result<QByteArray> networkFetch(RestClient* client, const QString& url) {
    const QUrl full(url);
    QUrl base;
    base.setScheme(full.scheme());
    base.setHost(full.host());
    if (full.port(-1) != -1)
        base.setPort(full.port());
    return client->getBytes(base, full.path(), QUrlQuery(full.query())).result();
}

} // namespace

// ---------------------------------------------------------------------------
// PodcastManager
// ---------------------------------------------------------------------------

PodcastManager::PodcastManager(QObject* parent)
    : QObject(parent)
    , m_parser(new PodcastFeedParser(this))
    , m_store(new PodcastStore(this))
{
    // RestClient is created lazily inside the default fetcher so that tests
    // which inject stub fetchers never instantiate QNetworkAccessManager.
    m_feedFetcher = [this](const QString& url) -> Result<QByteArray> {
        if (!m_restClient)
            m_restClient = new RestClient(this);
        return networkFetch(m_restClient, url);
    };
    m_enclosureFetcher = m_feedFetcher;
}

PodcastManager::~PodcastManager() = default;

void PodcastManager::setFeedFetcher(FeedFetcher fetcher) {
    m_feedFetcher = std::move(fetcher);
}

void PodcastManager::setEnclosureFetcher(FeedFetcher fetcher) {
    m_enclosureFetcher = std::move(fetcher);
}

Result<int> PodcastManager::subscribe(const QString& url) {
    qCDebug(lcPodcastMgr) << "subscribe:" << url;

    auto fetchResult = m_feedFetcher(url);
    if (fetchResult.isErr()) {
        const QString msg = tr("Failed to fetch feed '%1': %2")
            .arg(url, fetchResult.error().message);
        qCDebug(lcPodcastMgr) << msg;
        emit errorOccurred(msg);
        return fetchResult.error();
    }

    auto parseResult = m_parser->parseBytes(fetchResult.value(), url);
    if (parseResult.isErr()) {
        const QString msg = tr("Failed to parse feed '%1': %2")
            .arg(url, parseResult.error().message);
        qCDebug(lcPodcastMgr) << msg;
        emit errorOccurred(msg);
        return parseResult.error();
    }

    auto subResult = m_store->subscribe(url);
    if (subResult.isErr()) {
        emit errorOccurred(subResult.error().message);
        return subResult.error();
    }
    const int feedId = subResult.value();

    if (auto r = m_store->updateFeedMetadata(feedId, parseResult.value()); r.isErr())
        qCDebug(lcPodcastMgr) << "updateFeedMetadata failed:" << r.error().message;

    auto upsertResult = m_store->upsertEpisodes(feedId, parseResult.value().episodes);
    if (upsertResult.isErr()) {
        emit errorOccurred(upsertResult.error().message);
        return upsertResult.error();
    }

    qCDebug(lcPodcastMgr) << "subscribe OK: feedId=" << feedId
                       << "newEpisodes=" << upsertResult.value();
    return Result<int>::ok(feedId);
}

Result<int> PodcastManager::refreshFeed(int feedId) {
    qCDebug(lcPodcastMgr) << "refreshFeed: feedId=" << feedId;

    auto feedResult = m_store->feed(feedId);
    if (feedResult.isErr()) {
        emit errorOccurred(feedResult.error().message);
        return feedResult.error();
    }
    if (!feedResult.value().has_value()) {
        const QString msg = tr("Feed id %1 not found.").arg(feedId);
        emit errorOccurred(msg);
        return Result<int>::err(Error::InvalidArgument, msg);
    }
    const QString url = feedResult.value()->url;

    auto fetchResult = m_feedFetcher(url);
    if (fetchResult.isErr()) {
        const QString msg = tr("Failed to fetch feed '%1': %2")
            .arg(url, fetchResult.error().message);
        qCDebug(lcPodcastMgr) << msg;
        emit errorOccurred(msg);
        return fetchResult.error();
    }

    auto parseResult = m_parser->parseBytes(fetchResult.value(), url);
    if (parseResult.isErr()) {
        const QString msg = tr("Failed to parse feed '%1': %2")
            .arg(url, parseResult.error().message);
        qCDebug(lcPodcastMgr) << msg;
        emit errorOccurred(msg);
        return parseResult.error();
    }

    if (auto r = m_store->updateFeedMetadata(feedId, parseResult.value()); r.isErr())
        qCDebug(lcPodcastMgr) << "updateFeedMetadata failed:" << r.error().message;

    auto upsertResult = m_store->upsertEpisodes(feedId, parseResult.value().episodes);
    if (upsertResult.isErr()) {
        emit errorOccurred(upsertResult.error().message);
        return upsertResult.error();
    }

    const int newEpisodes = upsertResult.value();
    qCDebug(lcPodcastMgr) << "refreshFeed OK: feedId=" << feedId
                       << "newEpisodes=" << newEpisodes;
    emit feedRefreshed(feedId, newEpisodes);
    return Result<int>::ok(newEpisodes);
}

Result<int> PodcastManager::refreshAll() {
    qCDebug(lcPodcastMgr) << "refreshAll";

    auto feedsResult = m_store->feeds();
    if (feedsResult.isErr()) {
        emit errorOccurred(feedsResult.error().message);
        return feedsResult.error();
    }

    int totalNew = 0;
    for (const auto& feed : feedsResult.value()) {
        auto r = refreshFeed(feed.id);
        if (r.isErr()) {
            qCDebug(lcPodcastMgr) << "refreshAll: feed" << feed.id
                               << "failed:" << r.error().message;
            continue;
        }
        totalNew += r.value();
    }
    qCDebug(lcPodcastMgr) << "refreshAll done: totalNew=" << totalNew;
    return Result<int>::ok(totalNew);
}

Result<QString> PodcastManager::downloadEpisode(int episodeId, const QString& targetDir) {
    qCDebug(lcPodcastMgr) << "downloadEpisode: episodeId=" << episodeId
                       << "targetDir=" << targetDir;

    auto epResult = m_store->episode(episodeId);
    if (epResult.isErr()) {
        emit errorOccurred(epResult.error().message);
        return epResult.error();
    }
    if (!epResult.value().has_value()) {
        const QString msg = tr("Episode id %1 not found.").arg(episodeId);
        emit errorOccurred(msg);
        return Result<QString>::err(Error::InvalidArgument, msg);
    }
    const auto& ep = *epResult.value();

    if (ep.enclosureUrl.isEmpty()) {
        const QString msg = tr("Episode %1 has no enclosure URL.").arg(episodeId);
        emit errorOccurred(msg);
        return Result<QString>::err(Error::InvalidArgument, msg);
    }

    auto fetchResult = m_enclosureFetcher(ep.enclosureUrl);
    if (fetchResult.isErr()) {
        const QString msg = tr("Failed to download episode %1: %2")
            .arg(episodeId).arg(fetchResult.error().message);
        emit errorOccurred(msg);
        return fetchResult.error();
    }

    // Derive file extension from MIME type, then URL extension, then fall back.
    QString ext = mimeToExtension(ep.enclosureType);
    if (ext.isEmpty()) {
        const QString urlExt = QFileInfo(QUrl(ep.enclosureUrl).path()).suffix();
        ext = urlExt.isEmpty() ? QStringLiteral(".mp3")
                               : QStringLiteral(".") + urlExt;
    }

    const QString baseName = safeFilename(ep.title, ep.guid);
    const QString absPath  = QDir(targetDir).absoluteFilePath(baseName + ext);

    QFile file(absPath);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString msg = tr("Cannot write episode to '%1'.").arg(absPath);
        emit errorOccurred(msg);
        return Result<QString>::err(Error::FileAccessDenied, msg);
    }
    file.write(fetchResult.value());
    file.close();

    if (auto r = m_store->setLocalPath(episodeId, absPath); r.isErr())
        qCDebug(lcPodcastMgr) << "setLocalPath failed:" << r.error().message;

    qCDebug(lcPodcastMgr) << "downloadEpisode OK: path=" << absPath;
    emit episodeDownloaded(episodeId, absPath);
    return Result<QString>::ok(absPath);
}

} // namespace soundshelf
