#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/network/RestClient.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Client for a remote SoundShelf headless server.
 *
 * Mirrors the REST endpoints exposed by @ref HttpServer:
 *  - `GET /api/v1/tracks?q=...&limit=...`
 *  - `GET /api/v1/tracks/<id>`
 *  - `GET /api/v1/discs`
 *  - `GET /api/v1/stream/<trackId>` (audio bytes)
 *
 * Authentication is bearer-token only — set with @ref setBearerToken;
 * the server rejects anonymous requests.
 */
class NetworkLibrary : public QObject {
    Q_OBJECT
public:
    explicit NetworkLibrary(QObject* parent = nullptr);
    ~NetworkLibrary() override;

    void setBaseUrl(const QUrl& url) { m_base = url; }
    QUrl baseUrl() const { return m_base; }

    void setBearerToken(const QString& token) { m_token = token; }

    QFuture<Result<QJsonDocument>> searchTracks(const QString& query, int limit = 100);
    QFuture<Result<QJsonDocument>> getTrack(int id);
    QFuture<Result<QJsonDocument>> listDiscs(int limit = 100);

    /// Streaming URL for @p trackId. Embeds the bearer token if any —
    /// suitable for handing straight to libmpv.
    QUrl streamUrl(int trackId) const;

private:
    RestClient m_rest;
    QUrl       m_base;
    QString    m_token;
};

} // namespace soundshelf
