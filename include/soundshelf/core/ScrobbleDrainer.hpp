#pragma once

#include <QObject>
#include <QTimer>

namespace soundshelf {

class Scrobbler;
class LastFmClient;
class ListenBrainzClient;

/**
 * @brief Drains @ref Scrobbler's pending queue to the network clients.
 *
 * @ref Scrobbler is offline-first: every completed track adds a row to
 * `scrobble_queue` and emits @c scrobbleEnqueued. The drainer hooks
 * that signal *and* runs a periodic timer (default 60 s) so:
 *
 *  - immediate: a queued row triggers a send straight away (subject
 *    to throttling),
 *  - eventual: anything that failed earlier is retried at the
 *    interval until @c retry_count hits 5 (limit enforced by
 *    Scrobbler::pendingRows).
 *
 * Each pending row maps to one of two clients by its @c service
 * column: `lastfm` → @ref LastFmClient::scrobble, `listenbrainz` →
 * @ref ListenBrainzClient::submitListen. Success → markSent, failure
 * → markFailed (which bumps @c retry_count). The drainer doesn't
 * resolve OAuth or network availability — that's the clients' job.
 */
class ScrobbleDrainer : public QObject {
    Q_OBJECT
public:
    ScrobbleDrainer(Scrobbler* scrobbler,
                    LastFmClient* lastfm,
                    ListenBrainzClient* listenbrainz,
                    QObject* parent = nullptr);
    ~ScrobbleDrainer() override;

    void setIntervalSec(int seconds);
    int  intervalSec() const { return m_timer.interval() / 1000; }

    /// Starts periodic draining and binds to Scrobbler's enqueued signal.
    void start();

    /// Stops both the timer and the immediate-drain hook.
    void stop();

    /// Number of queue rows the next tick will pull at most.
    void setBatchSize(int n) { m_batchSize = n; }

public slots:
    /// Drain right now (also called by the timer / enqueue hook).
    void drainOnce();

signals:
    /// Emitted after each row finishes — @p ok mirrors the network
    /// outcome. Useful for status-bar feedback.
    void rowDrained(int queueRowId, bool ok);

private:
    void sendOne(int rowId, int trackId, const QString& service,
                 qint64 timestamp);

    Scrobbler*          m_scrobbler = nullptr;
    LastFmClient*       m_lastfm = nullptr;
    ListenBrainzClient* m_listenbrainz = nullptr;
    QTimer              m_timer;
    int                 m_batchSize = 25;
    bool                m_running = false;
};

} // namespace soundshelf
