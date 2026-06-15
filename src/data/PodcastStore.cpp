#include "soundshelf/data/PodcastStore.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

Q_LOGGING_CATEGORY(lcPodcastStore, "soundshelf.data")

namespace soundshelf {

PodcastStore::PodcastStore(QObject* parent) : QObject(parent) {}
PodcastStore::~PodcastStore() = default;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static PodcastStore::Feed rowToFeed(QSqlQuery& q) {
    PodcastStore::Feed f;
    f.id          = q.value(0).toInt();
    f.url         = q.value(1).toString();
    f.title       = q.value(2).toString();
    f.author      = q.value(3).toString();
    f.description = q.value(4).toString();
    f.imageUrl    = q.value(5).toString();
    f.link        = q.value(6).toString();
    f.language    = q.value(7).toString();
    f.lastRefreshed = q.value(8).toDateTime();
    f.addedAt       = q.value(9).toDateTime();
    return f;
}

static PodcastStore::Episode rowToEpisode(QSqlQuery& q) {
    PodcastStore::Episode e;
    e.id              = q.value(0).toInt();
    e.feedId          = q.value(1).toInt();
    e.guid            = q.value(2).toString();
    e.title           = q.value(3).toString();
    e.description     = q.value(4).toString();
    e.enclosureUrl    = q.value(5).toString();
    e.enclosureType   = q.value(6).toString();
    e.enclosureLength = q.value(7).toLongLong();
    e.pubDate         = q.value(8).toDateTime();
    e.durationMs      = q.value(9).toInt();
    e.episodeNumber   = q.value(10).toInt();
    e.isPlayed        = q.value(11).toInt() != 0;
    e.localPath       = q.value(12).toString();
    return e;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Result<int> PodcastStore::subscribe(const QString& url) {
    auto db = DatabaseManager::instance().database();

    // Check for existing row first (idempotent).
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT id FROM podcast_feeds WHERE url = ?"));
    sel.addBindValue(url);
    if (!sel.exec()) {
        return Result<int>::err(Error::DatabaseError, sel.lastError().text());
    }
    if (sel.next()) {
        const int existing = sel.value(0).toInt();
        qCDebug(lcPodcastStore) << "PodcastStore: subscribe existing feed id" << existing << url;
        return Result<int>::ok(existing);
    }

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral("INSERT INTO podcast_feeds(url) VALUES (?)"));
    ins.addBindValue(url);
    if (!ins.exec()) {
        return Result<int>::err(Error::DatabaseError, ins.lastError().text());
    }
    const int newId = ins.lastInsertId().toInt();
    qCDebug(lcPodcastStore) << "PodcastStore: subscribed new feed id" << newId << url;
    return Result<int>::ok(newId);
}

Result<void> PodcastStore::updateFeedMetadata(int feedId, const PodcastFeedParser::Feed& parsed) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE podcast_feeds SET "
        "  title = ?, author = ?, description = ?, image_url = ?, "
        "  link = ?, language = ?, last_refreshed = ? "
        "WHERE id = ?"));
    q.addBindValue(parsed.title);
    q.addBindValue(parsed.author);
    q.addBindValue(parsed.description);
    q.addBindValue(parsed.imageUrl);
    q.addBindValue(parsed.link);
    q.addBindValue(parsed.language);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.addBindValue(feedId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    qCDebug(lcPodcastStore) << "PodcastStore: updated metadata for feed id" << feedId;
    return Result<void>::ok();
}

Result<int> PodcastStore::upsertEpisodes(int feedId,
                                          const QList<PodcastFeedParser::Episode>& episodes) {
    auto db = DatabaseManager::instance().database();

    // Count rows before insert to determine how many are genuinely new.
    QSqlQuery countBefore(db);
    countBefore.prepare(
        QStringLiteral("SELECT COUNT(*) FROM podcast_episodes WHERE feed_id = ?"));
    countBefore.addBindValue(feedId);
    if (!countBefore.exec() || !countBefore.next()) {
        return Result<int>::err(Error::DatabaseError, countBefore.lastError().text());
    }
    const int before = countBefore.value(0).toInt();

    // INSERT OR IGNORE for the unique (feed_id, guid) key to avoid touching
    // is_played / local_path, then UPDATE the mutable metadata columns
    // for the row that was either just inserted or already existed.
    // SQLite does not support ON CONFLICT DO UPDATE (UPSERT) that preserves
    // specific columns in older versions, so we use INSERT OR IGNORE + UPDATE.
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO podcast_episodes"
        "(feed_id, guid, title, description, enclosure_url, enclosure_type, "
        " enclosure_length, pub_date, duration_ms, episode_number) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral(
        "UPDATE podcast_episodes SET "
        "  title = ?, description = ?, enclosure_url = ?, enclosure_type = ?, "
        "  enclosure_length = ?, pub_date = ?, duration_ms = ?, episode_number = ? "
        "WHERE feed_id = ? AND guid = ?"));

    for (const auto& ep : episodes) {
        ins.addBindValue(feedId);
        ins.addBindValue(ep.guid);
        ins.addBindValue(ep.title);
        ins.addBindValue(ep.description);
        ins.addBindValue(ep.enclosureUrl);
        ins.addBindValue(ep.enclosureType);
        ins.addBindValue(ep.enclosureLength);
        ins.addBindValue(ep.pubDate.isValid()
                         ? QVariant(ep.pubDate.toString(Qt::ISODate))
                         : QVariant(QMetaType(QMetaType::QString)));
        ins.addBindValue(ep.durationMs);
        ins.addBindValue(ep.episodeNumber);
        if (!ins.exec()) {
            return Result<int>::err(Error::DatabaseError, ins.lastError().text());
        }

        upd.addBindValue(ep.title);
        upd.addBindValue(ep.description);
        upd.addBindValue(ep.enclosureUrl);
        upd.addBindValue(ep.enclosureType);
        upd.addBindValue(ep.enclosureLength);
        upd.addBindValue(ep.pubDate.isValid()
                         ? QVariant(ep.pubDate.toString(Qt::ISODate))
                         : QVariant(QMetaType(QMetaType::QString)));
        upd.addBindValue(ep.durationMs);
        upd.addBindValue(ep.episodeNumber);
        upd.addBindValue(feedId);
        upd.addBindValue(ep.guid);
        if (!upd.exec()) {
            return Result<int>::err(Error::DatabaseError, upd.lastError().text());
        }
    }

    QSqlQuery countAfter(db);
    countAfter.prepare(
        QStringLiteral("SELECT COUNT(*) FROM podcast_episodes WHERE feed_id = ?"));
    countAfter.addBindValue(feedId);
    if (!countAfter.exec() || !countAfter.next()) {
        return Result<int>::err(Error::DatabaseError, countAfter.lastError().text());
    }
    const int after = countAfter.value(0).toInt();
    const int inserted = after - before;
    qCDebug(lcPodcastStore) << "PodcastStore: upsertEpisodes feed" << feedId
                       << "new:" << inserted << "total:" << after;
    return Result<int>::ok(inserted);
}

Result<QList<PodcastStore::Feed>> PodcastStore::feeds() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT id, url, title, author, description, image_url, link, language, "
            "       last_refreshed, added_at "
            "FROM podcast_feeds ORDER BY title ASC"))) {
        return Result<QList<Feed>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Feed> rows;
    while (q.next()) rows.append(rowToFeed(q));
    return Result<QList<Feed>>::ok(std::move(rows));
}

Result<std::optional<PodcastStore::Feed>> PodcastStore::feed(int feedId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, url, title, author, description, image_url, link, language, "
        "       last_refreshed, added_at "
        "FROM podcast_feeds WHERE id = ?"));
    q.addBindValue(feedId);
    if (!q.exec()) {
        return Result<std::optional<Feed>>::err(Error::DatabaseError, q.lastError().text());
    }
    if (!q.next()) return Result<std::optional<Feed>>::ok(std::nullopt);
    return Result<std::optional<Feed>>::ok(rowToFeed(q));
}

Result<QList<PodcastStore::Episode>> PodcastStore::episodesForFeed(int feedId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, feed_id, guid, title, description, enclosure_url, enclosure_type, "
        "       enclosure_length, pub_date, duration_ms, episode_number, is_played, local_path "
        "FROM podcast_episodes WHERE feed_id = ? "
        "ORDER BY pub_date DESC NULLS LAST"));
    q.addBindValue(feedId);
    if (!q.exec()) {
        return Result<QList<Episode>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Episode> rows;
    while (q.next()) rows.append(rowToEpisode(q));
    return Result<QList<Episode>>::ok(std::move(rows));
}

Result<std::optional<PodcastStore::Episode>> PodcastStore::episode(int episodeId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, feed_id, guid, title, description, enclosure_url, enclosure_type, "
        "       enclosure_length, pub_date, duration_ms, episode_number, is_played, local_path "
        "FROM podcast_episodes WHERE id = ?"));
    q.addBindValue(episodeId);
    if (!q.exec()) {
        return Result<std::optional<Episode>>::err(Error::DatabaseError, q.lastError().text());
    }
    if (!q.next())
        return Result<std::optional<Episode>>::ok(std::nullopt);
    return Result<std::optional<Episode>>::ok(rowToEpisode(q));
}

Result<void> PodcastStore::setPlayed(int episodeId, bool played) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE podcast_episodes SET is_played = ? WHERE id = ?"));
    q.addBindValue(played ? 1 : 0);
    q.addBindValue(episodeId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    qCDebug(lcPodcastStore) << "PodcastStore: setPlayed episode" << episodeId << played;
    return Result<void>::ok();
}

Result<void> PodcastStore::setLocalPath(int episodeId, const QString& path) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE podcast_episodes SET local_path = ? WHERE id = ?"));
    q.addBindValue(path.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(path));
    q.addBindValue(episodeId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    qCDebug(lcPodcastStore) << "PodcastStore: setLocalPath episode" << episodeId << path;
    return Result<void>::ok();
}

Result<void> PodcastStore::unsubscribe(int feedId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM podcast_feeds WHERE id = ?"));
    q.addBindValue(feedId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    qCDebug(lcPodcastStore) << "PodcastStore: unsubscribed feed id" << feedId;
    return Result<void>::ok();
}

} // namespace soundshelf
