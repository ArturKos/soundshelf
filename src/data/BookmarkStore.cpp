#include "soundshelf/data/BookmarkStore.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBookmark, "soundshelf.bookmark")

namespace soundshelf {

BookmarkStore::BookmarkStore(QObject* parent) : QObject(parent) {}
BookmarkStore::~BookmarkStore() = default;

Result<void> BookmarkStore::setResumePosition(int trackId, int positionMs) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery del(db);
    del.prepare(QStringLiteral(
        "DELETE FROM bookmarks WHERE track_id = ? AND is_resume = 1"));
    del.addBindValue(trackId);
    if (!del.exec()) {
        return Result<void>::err(Error::DatabaseError,
            tr("bookmark delete resume: %1").arg(del.lastError().text()));
    }

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO bookmarks(track_id, position_ms, is_resume) VALUES (?, ?, 1)"));
    ins.addBindValue(trackId);
    ins.addBindValue(positionMs);
    if (!ins.exec()) {
        return Result<void>::err(Error::DatabaseError,
            tr("bookmark insert resume: %1").arg(ins.lastError().text()));
    }

    qCDebug(lcBookmark) << "Resume set for track" << trackId << "at" << positionMs << "ms";
    return Result<void>::ok();
}

Result<std::optional<int>> BookmarkStore::resumePosition(int trackId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT position_ms FROM bookmarks WHERE track_id = ? AND is_resume = 1"));
    q.addBindValue(trackId);
    if (!q.exec()) {
        return Result<std::optional<int>>::err(Error::DatabaseError, q.lastError().text());
    }
    if (!q.next()) {
        return Result<std::optional<int>>::ok(std::nullopt);
    }
    return Result<std::optional<int>>::ok(q.value(0).toInt());
}

Result<void> BookmarkStore::clearResume(int trackId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "DELETE FROM bookmarks WHERE track_id = ? AND is_resume = 1"));
    q.addBindValue(trackId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError,
            tr("bookmark clear resume: %1").arg(q.lastError().text()));
    }
    qCDebug(lcBookmark) << "Resume cleared for track" << trackId;
    return Result<void>::ok();
}

Result<int> BookmarkStore::addBookmark(int trackId, int positionMs, const QString& label) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO bookmarks(track_id, position_ms, label, is_resume) VALUES (?, ?, ?, 0)"));
    q.addBindValue(trackId);
    q.addBindValue(positionMs);
    q.addBindValue(label.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(label));
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            tr("bookmark insert: %1").arg(q.lastError().text()));
    }
    const int id = q.lastInsertId().toInt();
    qCDebug(lcBookmark) << "Bookmark added id" << id << "track" << trackId << "pos" << positionMs;
    return Result<int>::ok(id);
}

Result<QList<BookmarkStore::Bookmark>> BookmarkStore::bookmarksForTrack(int trackId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, track_id, position_ms, label, is_resume, created_at "
        "FROM bookmarks WHERE track_id = ? AND is_resume = 0 "
        "ORDER BY position_ms ASC"));
    q.addBindValue(trackId);
    if (!q.exec()) {
        return Result<QList<Bookmark>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Bookmark> rows;
    while (q.next()) {
        Bookmark b;
        b.id = q.value(0).toInt();
        b.trackId = q.value(1).toInt();
        b.positionMs = q.value(2).toInt();
        b.label = q.value(3).toString();
        b.isResume = q.value(4).toInt() != 0;
        b.createdAt = q.value(5).toDateTime();
        rows.append(b);
    }
    return Result<QList<Bookmark>>::ok(std::move(rows));
}

Result<void> BookmarkStore::removeBookmark(int id) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError,
            tr("bookmark remove: %1").arg(q.lastError().text()));
    }
    qCDebug(lcBookmark) << "Bookmark removed id" << id;
    return Result<void>::ok();
}

Result<int> BookmarkStore::removeAllForTrack(int trackId) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE track_id = ?"));
    q.addBindValue(trackId);
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            tr("bookmark remove all: %1").arg(q.lastError().text()));
    }
    const int deleted = q.numRowsAffected();
    qCDebug(lcBookmark) << "Removed" << deleted << "bookmarks for track" << trackId;
    return Result<int>::ok(deleted);
}

} // namespace soundshelf
