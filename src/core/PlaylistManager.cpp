#include "soundshelf/core/PlaylistManager.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPl, "soundshelf.core.playlist")

namespace soundshelf {

PlaylistManager::PlaylistManager(QObject* parent) : QObject(parent) {}
PlaylistManager::~PlaylistManager() = default;

namespace {

QSqlDatabase db() { return DatabaseManager::instance().database(); }

} // namespace

Result<int> PlaylistManager::create(const QString& name) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO playlists(name, type) VALUES (?, 'manual')"));
    q.addBindValue(name);
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError, q.lastError().text());
    }
    const int id = q.lastInsertId().toInt();
    emit playlistCreated(id);
    return Result<int>::ok(id);
}

Result<void> PlaylistManager::rename(int id, const QString& newName) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE playlists SET name = ? WHERE id = ?"));
    q.addBindValue(newName);
    q.addBindValue(id);
    if (!q.exec()) return Result<void>::err(Error::DatabaseError, q.lastError().text());
    emit playlistChanged(id);
    return Result<void>::ok();
}

Result<void> PlaylistManager::remove(int id) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM playlists WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) return Result<void>::err(Error::DatabaseError, q.lastError().text());
    emit playlistRemoved(id);
    return Result<void>::ok();
}

Result<QList<PlaylistManager::Playlist>> PlaylistManager::list() {
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, name, type, smart_rules_json, created_at "
        "FROM playlists ORDER BY name"));
    if (!q.exec()) {
        return Result<QList<Playlist>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Playlist> rows;
    while (q.next()) {
        Playlist p;
        p.id = q.value(0).toInt();
        p.name = q.value(1).toString();
        p.smart = q.value(2).toString() == QLatin1String("smart");
        p.rulesJson = q.value(3).toString();
        p.createdAt = q.value(4).toDateTime();
        rows.append(p);
    }
    return Result<QList<Playlist>>::ok(std::move(rows));
}

Result<PlaylistManager::Playlist> PlaylistManager::load(int id) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, name, type, smart_rules_json, created_at "
        "FROM playlists WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        return Result<Playlist>::err(Error::DatabaseError,
            QStringLiteral("Playlist %1 not found").arg(id));
    }
    Playlist p;
    p.id = q.value(0).toInt();
    p.name = q.value(1).toString();
    p.smart = q.value(2).toString() == QLatin1String("smart");
    p.rulesJson = q.value(3).toString();
    p.createdAt = q.value(4).toDateTime();
    return Result<Playlist>::ok(std::move(p));
}

Result<void> PlaylistManager::appendTrack(int playlistId, int trackId) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO playlist_tracks(playlist_id, track_id, position) "
        "VALUES (?, ?, "
        "COALESCE((SELECT MAX(position) + 1 FROM playlist_tracks WHERE playlist_id = ?), 0))"));
    q.addBindValue(playlistId);
    q.addBindValue(trackId);
    q.addBindValue(playlistId);
    if (!q.exec()) return Result<void>::err(Error::DatabaseError, q.lastError().text());
    emit playlistChanged(playlistId);
    return Result<void>::ok();
}

Result<void> PlaylistManager::setTracks(int playlistId, const QList<int>& trackIds) {
    auto database = db();
    if (!database.transaction()) {
        return Result<void>::err(Error::DatabaseError,
            QStringLiteral("Cannot start transaction"));
    }
    QSqlQuery del(database);
    del.prepare(QStringLiteral("DELETE FROM playlist_tracks WHERE playlist_id = ?"));
    del.addBindValue(playlistId);
    if (!del.exec()) {
        database.rollback();
        return Result<void>::err(Error::DatabaseError, del.lastError().text());
    }
    QSqlQuery ins(database);
    ins.prepare(QStringLiteral(
        "INSERT INTO playlist_tracks(playlist_id, track_id, position) VALUES (?, ?, ?)"));
    for (int i = 0; i < trackIds.size(); ++i) {
        ins.bindValue(0, playlistId);
        ins.bindValue(1, trackIds[i]);
        ins.bindValue(2, i);
        if (!ins.exec()) {
            database.rollback();
            return Result<void>::err(Error::DatabaseError, ins.lastError().text());
        }
    }
    if (!database.commit()) {
        return Result<void>::err(Error::DatabaseError,
            QStringLiteral("Commit failed"));
    }
    emit playlistChanged(playlistId);
    return Result<void>::ok();
}

Result<QList<Track>> PlaylistManager::tracksOf(int playlistId) {
    auto& dbm = DatabaseManager::instance();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? "
        "ORDER BY position ASC"));
    q.addBindValue(playlistId);
    if (!q.exec()) {
        return Result<QList<Track>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Track> out;
    while (q.next()) {
        const int tid = q.value(0).toInt();
        if (auto t = dbm.getTrack(tid); t) out.append(t.value());
    }
    return Result<QList<Track>>::ok(std::move(out));
}

Result<int> PlaylistManager::createSmart(const QString& name, const QString& rulesJson) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO playlists(name, type, smart_rules_json) VALUES (?, 'smart', ?)"));
    q.addBindValue(name);
    q.addBindValue(rulesJson);
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError, q.lastError().text());
    }
    const int id = q.lastInsertId().toInt();
    emit playlistCreated(id);
    return Result<int>::ok(id);
}

Result<void> PlaylistManager::updateSmartRules(int id, const QString& rulesJson) {
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE playlists SET smart_rules_json = ? WHERE id = ? AND type = 'smart'"));
    q.addBindValue(rulesJson);
    q.addBindValue(id);
    if (!q.exec()) return Result<void>::err(Error::DatabaseError, q.lastError().text());
    emit playlistChanged(id);
    return Result<void>::ok();
}

// ---- Runtime queue ----

QList<Track> PlaylistManager::queue() const { return m_queue; }
int PlaylistManager::queueIndex() const { return m_queueIndex; }

void PlaylistManager::setQueue(const QList<Track>& q, int startIndex) {
    m_queue = q;
    m_queueIndex = m_queue.isEmpty() ? -1 : qBound(0, startIndex, m_queue.size() - 1);
    emit queueChanged();
    emit queueIndexChanged(m_queueIndex);
}

void PlaylistManager::appendToQueue(const Track& t) {
    m_queue.append(t);
    if (m_queueIndex < 0) m_queueIndex = 0;
    emit queueChanged();
}

void PlaylistManager::clearQueue() {
    m_queue.clear();
    m_queueIndex = -1;
    emit queueChanged();
    emit queueIndexChanged(-1);
}

bool PlaylistManager::advanceQueue() {
    if (m_queueIndex + 1 >= m_queue.size()) return false;
    ++m_queueIndex;
    emit queueIndexChanged(m_queueIndex);
    return true;
}

bool PlaylistManager::retreatQueue() {
    if (m_queueIndex <= 0) return false;
    --m_queueIndex;
    emit queueIndexChanged(m_queueIndex);
    return true;
}

void PlaylistManager::setQueueIndex(int idx) {
    if (idx < 0 || idx >= m_queue.size()) return;
    if (idx == m_queueIndex) return;
    m_queueIndex = idx;
    emit queueIndexChanged(m_queueIndex);
}

} // namespace soundshelf
