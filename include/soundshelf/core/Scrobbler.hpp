#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Persists "now-playing" + completed-track events to the
 * scrobble services (Last.fm and ListenBrainz).
 *
 * The class itself does **not** speak HTTP — it only manages the
 * `scrobble_queue` SQL table (insert on completed plays, mark as sent
 * after a network attempt) and emits signals that
 * @ref network::LastFmClient / @ref network::ListenBrainzClient pick
 * up. This split keeps offline support trivial: the queue lives in
 * SQLite, the network clients drain it whenever they're online.
 *
 * Per Last.fm rules a track only counts as scrobbled when:
 *  - it is longer than 30 s, AND
 *  - the user has played at least 50% of it OR played 4 minutes,
 *  whichever comes first.
 */
class Scrobbler : public QObject {
    Q_OBJECT
public:
    explicit Scrobbler(QObject* parent = nullptr);
    ~Scrobbler() override;

    /// Enables/disables the integration globally.
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /// Per-service toggles.
    void setLastFmEnabled(bool e)        { m_lastfm = e; }
    void setListenBrainzEnabled(bool e)  { m_listenbrainz = e; }
    bool lastFmEnabled() const           { return m_lastfm; }
    bool listenBrainzEnabled() const     { return m_listenbrainz; }

    /// Hook from the player when a track starts.
    void onTrackStarted(const Track& t);

    /// Hook from the player on each position update (in ms).
    void onPositionTick(int posMs);

    /// Hook from the player when the track ends.
    /// `playedMs` should be the actual playback time the user heard.
    /// `completed` is true if the track played to its end.
    void onTrackEnded(const Track& t, int playedMs, bool completed);

    /// Drains the queue: returns the rows that the network clients
    /// should send. Call repeatedly until the result is empty.
    struct QueueRow {
        int id = -1;
        int trackId = -1;
        QString service;
    };
    Result<QList<QueueRow>> pendingRows(int limit = 50);

    /// Marks a queue row as successfully sent.
    Result<void> markSent(int queueRowId, const QString& response = {});

    /// Increments retry counter on failure.
    Result<void> markFailed(int queueRowId, const QString& response);

signals:
    /// Emitted when a row was just enqueued — wakeup signal for clients.
    void scrobbleEnqueued(int queueRowId);

    /// Emitted when the user starts a track. Network clients should
    /// post `track.updateNowPlaying` (Last.fm) at this point.
    void nowPlaying(const Track& t);

private:
    bool        m_enabled = true;
    bool        m_lastfm = false;
    bool        m_listenbrainz = false;
    Track       m_currentTrack;
    int         m_currentPosMs = 0;
    bool        m_currentScrobbled = false;
};

} // namespace soundshelf
