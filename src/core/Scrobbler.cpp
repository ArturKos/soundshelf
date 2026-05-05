#include "soundshelf/core/Scrobbler.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcScr, "soundshelf.core.scrobble")

namespace soundshelf {

Scrobbler::Scrobbler(QObject* parent) : QObject(parent) {}
Scrobbler::~Scrobbler() = default;

void Scrobbler::setEnabled(bool enabled) { m_enabled = enabled; }

void Scrobbler::onTrackStarted(const Track& t) {
    m_currentTrack = t;
    m_currentPosMs = 0;
    m_currentScrobbled = false;
    if (m_enabled && (m_lastfm || m_listenbrainz)) emit nowPlaying(t);
}

void Scrobbler::onPositionTick(int posMs) {
    m_currentPosMs = posMs;
    if (!m_enabled || m_currentScrobbled) return;
    if (m_currentTrack.id < 0 || m_currentTrack.durationMs <= 0) return;

    // Last.fm rule: > 30 s long, played > 50% or > 4 minutes.
    if (m_currentTrack.durationMs < 30000) return;
    const int half = m_currentTrack.durationMs / 2;
    const int fourMin = 4 * 60 * 1000;
    if (posMs >= half || posMs >= fourMin) {
        // Don't actually scrobble until track ends — but mark threshold reached.
        // Final enqueue happens in onTrackEnded.
        m_currentScrobbled = true;
    }
}

void Scrobbler::onTrackEnded(const Track& t, int playedMs, bool completed) {
    if (!m_enabled) return;
    if (t.id < 0) return;
    // Enqueue if either the threshold was hit during play, or the track
    // genuinely completed.
    const bool eligible = m_currentScrobbled || completed;
    if (!eligible) return;
    if (playedMs < 30000) return;

    auto db = DatabaseManager::instance().database();
    auto enqueue = [&](const QString& service) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO scrobble_queue(track_id, service) VALUES (?, ?)"));
        q.addBindValue(t.id);
        q.addBindValue(service);
        if (!q.exec()) {
            qCWarning(lcScr) << "enqueue" << service << ":" << q.lastError().text();
            return;
        }
        emit scrobbleEnqueued(q.lastInsertId().toInt());
    };

    if (m_lastfm)        enqueue(QStringLiteral("lastfm"));
    if (m_listenbrainz)  enqueue(QStringLiteral("listenbrainz"));
}

Result<QList<Scrobbler::QueueRow>> Scrobbler::pendingRows(int limit) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, track_id, service, queued_at FROM scrobble_queue "
        "WHERE sent = 0 AND retry_count < 5 "
        "ORDER BY queued_at ASC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<QueueRow>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<QueueRow> out;
    while (q.next()) {
        QueueRow r;
        r.id = q.value(0).toInt();
        r.trackId = q.value(1).toInt();
        r.service = q.value(2).toString();
        r.queuedAt = q.value(3).toDateTime();
        out.append(r);
    }
    return Result<QList<QueueRow>>::ok(std::move(out));
}

Result<void> Scrobbler::markSent(int queueRowId, const QString& response) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE scrobble_queue SET sent = 1, response = ? WHERE id = ?"));
    q.addBindValue(response);
    q.addBindValue(queueRowId);
    if (!q.exec()) return Result<void>::err(Error::DatabaseError, q.lastError().text());
    return Result<void>::ok();
}

Result<void> Scrobbler::markFailed(int queueRowId, const QString& response) {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE scrobble_queue SET retry_count = retry_count + 1, response = ? "
        "WHERE id = ?"));
    q.addBindValue(response);
    q.addBindValue(queueRowId);
    if (!q.exec()) return Result<void>::err(Error::DatabaseError, q.lastError().text());
    return Result<void>::ok();
}

} // namespace soundshelf
