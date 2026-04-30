#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/network/RestClient.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Client for Last.fm's scrobble + now-playing API.
 *
 * Endpoint: `https://ws.audioscrobbler.com/2.0/`. Authentication is
 * web-flow OAuth — once the user has approved the app, we keep a
 * `session key` (`sk`) in the system keyring and sign each request
 * with `api_sig = md5(sorted-params + secret)`.
 *
 * The class doesn't manage the OAuth dance directly (that runs through
 * a browser); it only signs and POSTs against the Audioscrobbler v2
 * API.
 */
class LastFmClient : public QObject {
    Q_OBJECT
public:
    explicit LastFmClient(QObject* parent = nullptr);
    ~LastFmClient() override;

    /// Required: API key + secret from `https://www.last.fm/api/account/create`.
    void setApiCredentials(const QString& apiKey, const QString& sharedSecret);

    /// Set after the user has completed the web auth flow.
    void setSessionKey(const QString& sk) { m_sk = sk; }

    /// Marks the current track as "now playing".
    QFuture<Result<QJsonDocument>> updateNowPlaying(const Track& t);

    /// Submits one scrobble. @p timestamp = unix seconds.
    QFuture<Result<QJsonDocument>> scrobble(const Track& t, qint64 timestamp);

    /// Builds the `api_sig` MD5 expected by every signed call.
    /// Public for testability.
    static QString signParams(const QMap<QString, QString>& params,
                              const QString& sharedSecret);

private:
    RestClient m_rest;
    QString    m_apiKey;
    QString    m_sharedSecret;
    QString    m_sk;
};

} // namespace soundshelf
