#include "soundshelf/core/ScrobbleDrainer.hpp"
#include "soundshelf/core/Scrobbler.hpp"
#include "soundshelf/core/Track.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/network/LastFmClient.hpp"
#include "soundshelf/network/ListenBrainzClient.hpp"

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDrain, "soundshelf.scrobble.drain")

namespace soundshelf {

ScrobbleDrainer::ScrobbleDrainer(Scrobbler* scrobbler,
                                 LastFmClient* lastfm,
                                 ListenBrainzClient* listenbrainz,
                                 QObject* parent)
    : QObject(parent),
      m_scrobbler(scrobbler),
      m_lastfm(lastfm),
      m_listenbrainz(listenbrainz)
{
    m_timer.setInterval(60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &ScrobbleDrainer::drainOnce);
}

ScrobbleDrainer::~ScrobbleDrainer() = default;

void ScrobbleDrainer::setIntervalSec(int seconds) {
    m_timer.setInterval(qMax(5, seconds) * 1000);
}

void ScrobbleDrainer::start() {
    if (m_running) return;
    m_running = true;
    m_timer.start();
    if (m_scrobbler) {
        // Whenever a row is freshly queued, attempt a drain right away
        // — the user just heard the song, the Last.fm Now Playing
        // window is still ~30 s wide. start() is guarded by m_running,
        // so this connect runs only once per object lifetime.
        connect(m_scrobbler, &Scrobbler::scrobbleEnqueued,
                this, [this](int /*id*/) { drainOnce(); });
    }
    qCInfo(lcDrain) << "Started, interval =" << m_timer.interval() << "ms";
}

void ScrobbleDrainer::stop() {
    if (!m_running) return;
    m_timer.stop();
    if (m_scrobbler) disconnect(m_scrobbler, nullptr, this, nullptr);
    m_running = false;
}

void ScrobbleDrainer::drainOnce() {
    if (!m_scrobbler) return;
    auto pending = m_scrobbler->pendingRows(m_batchSize);
    if (!pending) {
        qCWarning(lcDrain) << "Cannot read queue:" << pending.error().message;
        return;
    }
    if (pending.value().isEmpty()) return;

    qCDebug(lcDrain) << "Draining" << pending.value().size() << "rows";
    for (const auto& row : pending.value()) {
        const qint64 ts = row.queuedAt.isValid()
            ? row.queuedAt.toSecsSinceEpoch()
            : QDateTime::currentSecsSinceEpoch();
        sendOne(row.id, row.trackId, row.service, ts);
    }
}

void ScrobbleDrainer::sendOne(int rowId, int trackId,
                              const QString& service, qint64 timestamp) {
    auto trackR = DatabaseManager::instance().getTrack(trackId);
    if (!trackR) {
        qCWarning(lcDrain) << "Cannot load track" << trackId
                           << ":" << trackR.error().message;
        m_scrobbler->markFailed(rowId, QStringLiteral("track gone"));
        emit rowDrained(rowId, false);
        return;
    }
    const Track t = trackR.value();

    QFuture<Result<QJsonDocument>> fut;
    if (service == QLatin1String("lastfm") && m_lastfm) {
        fut = m_lastfm->scrobble(t, timestamp);
    } else if (service == QLatin1String("listenbrainz") && m_listenbrainz) {
        fut = m_listenbrainz->submitListen(t, timestamp);
    } else {
        qCWarning(lcDrain) << "Unknown service" << service << "for row" << rowId;
        m_scrobbler->markFailed(rowId,
            QStringLiteral("unknown service: %1").arg(service));
        emit rowDrained(rowId, false);
        return;
    }

    auto* watcher = new QFutureWatcher<Result<QJsonDocument>>(this);
    connect(watcher, &QFutureWatcher<Result<QJsonDocument>>::finished,
            this, [this, watcher, rowId, service]() {
        const Result<QJsonDocument> r = watcher->result();
        watcher->deleteLater();
        if (r.isOk()) {
            qCInfo(lcDrain) << "Sent row" << rowId << "to" << service;
            m_scrobbler->markSent(rowId,
                QString::fromUtf8(r.value().toJson(QJsonDocument::Compact)));
            emit rowDrained(rowId, true);
        } else {
            qCWarning(lcDrain) << "Row" << rowId << service
                               << "failed:" << r.error().message;
            m_scrobbler->markFailed(rowId, r.error().message);
            emit rowDrained(rowId, false);
        }
    });
    watcher->setFuture(fut);
}

} // namespace soundshelf
