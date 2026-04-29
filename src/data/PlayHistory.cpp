#include "soundshelf/data/PlayHistory.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHist, "soundshelf.data.history")

namespace soundshelf {

PlayHistory::PlayHistory(QObject* parent) : QObject(parent) {}
PlayHistory::~PlayHistory() = default;

Result<int> PlayHistory::recordPlay(int trackId, int playedMs, bool completed, const QString& source) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO play_history(track_id, played_ms, completed, source) "
        "VALUES (?, ?, ?, ?)"));
    q.addBindValue(trackId);
    q.addBindValue(playedMs);
    q.addBindValue(completed ? 1 : 0);
    q.addBindValue(source);
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            QStringLiteral("history insert: %1").arg(q.lastError().text()));
    }
    const int id = q.lastInsertId().toInt();

    if (completed) {
        QSqlQuery up(db);
        up.prepare(QStringLiteral(
            "UPDATE tracks SET play_count = play_count + 1, "
            "last_played = CURRENT_TIMESTAMP WHERE id = ?"));
        up.addBindValue(trackId);
        if (!up.exec()) {
            qCWarning(lcHist) << "Cannot bump play_count:" << up.lastError().text();
        }
    }
    return Result<int>::ok(id);
}

Result<QList<PlayHistory::Entry>> PlayHistory::recent(int limit) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, track_id, played_at, played_ms, completed, source "
        "FROM play_history ORDER BY played_at DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<Entry>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Entry> rows;
    while (q.next()) {
        Entry e;
        e.id = q.value(0).toInt();
        e.trackId = q.value(1).toInt();
        e.playedAt = q.value(2).toDateTime();
        e.playedMs = q.value(3).toInt();
        e.completed = q.value(4).toInt() != 0;
        e.source = q.value(5).toString();
        rows.append(e);
    }
    return Result<QList<Entry>>::ok(std::move(rows));
}

Result<QList<PlayHistory::Entry>> PlayHistory::forTrack(int trackId, int limit) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, track_id, played_at, played_ms, completed, source "
        "FROM play_history WHERE track_id = ? "
        "ORDER BY played_at DESC LIMIT ?"));
    q.addBindValue(trackId);
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<Entry>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Entry> rows;
    while (q.next()) {
        Entry e;
        e.id = q.value(0).toInt();
        e.trackId = q.value(1).toInt();
        e.playedAt = q.value(2).toDateTime();
        e.playedMs = q.value(3).toInt();
        e.completed = q.value(4).toInt() != 0;
        e.source = q.value(5).toString();
        rows.append(e);
    }
    return Result<QList<Entry>>::ok(std::move(rows));
}

Result<QList<PlayHistory::Aggregate>> PlayHistory::topTracks(int limit, int sinceDays) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QString sql = QStringLiteral(
        "SELECT h.track_id, COUNT(*) AS plays, t.title, a.name "
        "FROM play_history h "
        "LEFT JOIN tracks t ON t.id = h.track_id "
        "LEFT JOIN artists a ON a.id = t.artist_id "
        "WHERE h.completed = 1 ");
    if (sinceDays > 0) {
        sql += QStringLiteral("AND h.played_at >= datetime('now', ?) ");
    }
    sql += QStringLiteral("GROUP BY h.track_id ORDER BY plays DESC LIMIT ?");
    q.prepare(sql);
    if (sinceDays > 0) {
        q.addBindValue(QStringLiteral("-%1 days").arg(sinceDays));
    }
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<Aggregate>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Aggregate> rows;
    while (q.next()) {
        Aggregate a;
        a.trackId = q.value(0).toInt();
        a.playCount = q.value(1).toInt();
        const QString title = q.value(2).toString();
        const QString artist = q.value(3).toString();
        a.label = artist.isEmpty() ? title : QStringLiteral("%1 — %2").arg(artist, title);
        rows.append(a);
    }
    return Result<QList<Aggregate>>::ok(std::move(rows));
}

Result<QList<int>> PlayHistory::playsPerWeekday(int sinceDays) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QString sql = QStringLiteral(
        "SELECT CAST(strftime('%w', played_at) AS INTEGER) AS dow, COUNT(*) "
        "FROM play_history WHERE completed = 1 ");
    if (sinceDays > 0) {
        sql += QStringLiteral("AND played_at >= datetime('now', ?) ");
    }
    sql += QStringLiteral("GROUP BY dow");
    q.prepare(sql);
    if (sinceDays > 0) {
        q.addBindValue(QStringLiteral("-%1 days").arg(sinceDays));
    }
    if (!q.exec()) {
        return Result<QList<int>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<int> result(7, 0);  // index 0 = Monday
    while (q.next()) {
        const int dowSqlite = q.value(0).toInt();    // 0 = Sunday in SQLite
        const int dowMon0   = (dowSqlite + 6) % 7;   // 0 = Monday
        result[dowMon0] = q.value(1).toInt();
    }
    return Result<QList<int>>::ok(std::move(result));
}

Result<qint64> PlayHistory::totalPlayedMs(int sinceDays) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QString sql = QStringLiteral("SELECT COALESCE(SUM(played_ms), 0) FROM play_history");
    if (sinceDays > 0) {
        sql += QStringLiteral(" WHERE played_at >= datetime('now', ?)");
    }
    q.prepare(sql);
    if (sinceDays > 0) q.addBindValue(QStringLiteral("-%1 days").arg(sinceDays));
    if (!q.exec() || !q.next()) {
        return Result<qint64>::err(Error::DatabaseError, q.lastError().text());
    }
    return Result<qint64>::ok(q.value(0).toLongLong());
}

Result<int> PlayHistory::prune(int olderThanDays) {
    if (olderThanDays <= 0) {
        return Result<int>::err(Error::InvalidArgument,
            QStringLiteral("prune needs positive olderThanDays"));
    }
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "DELETE FROM play_history WHERE played_at < datetime('now', ?)"));
    q.addBindValue(QStringLiteral("-%1 days").arg(olderThanDays));
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError, q.lastError().text());
    }
    return Result<int>::ok(q.numRowsAffected());
}

} // namespace soundshelf
