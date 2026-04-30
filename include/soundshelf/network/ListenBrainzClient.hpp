#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/network/RestClient.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Client for the ListenBrainz scrobble API.
 *
 * Endpoint: `https://api.listenbrainz.org/1/`. Authentication is a
 * single user token passed in the `Authorization: Token …` header.
 *
 * ListenBrainz accepts single and bulk submissions. Each submission
 * is a JSON envelope with a `listen_type` (`single` / `playing_now`
 * / `import`) and a `payload` array of one or more listen objects.
 */
class ListenBrainzClient : public QObject {
    Q_OBJECT
public:
    explicit ListenBrainzClient(QObject* parent = nullptr);
    ~ListenBrainzClient() override;

    void setUserToken(const QString& token) { m_token = token; }

    /// Submits one listen with `listen_type=single`.
    QFuture<Result<QJsonDocument>> submitListen(const Track& t, qint64 timestamp);

    /// Sends a `listen_type=playing_now` ping (no timestamp).
    QFuture<Result<QJsonDocument>> playingNow(const Track& t);

private:
    QByteArray buildEnvelope(const Track& t,
                             const QString& listenType,
                             qint64 timestamp) const;

    RestClient m_rest;
    QString    m_token;
};

} // namespace soundshelf
