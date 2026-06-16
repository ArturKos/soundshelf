#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QByteArray>
#include <QJsonObject>

#include "soundshelf/core/Result.hpp"
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/**
 * @brief POD representing a track as returned by the SoundShelf HTTP API.
 *
 * Field names and types mirror HttpServer::trackToJson exactly.
 * DB-local fields (disc id, cover hash) are absent — the API does not expose them.
 */
struct RemoteTrack {
    int     id          = 0;   ///< Server-side track id.
    QString title;             ///< Track title.
    QString artist;            ///< Track artist.
    QString album;             ///< Album name.
    qint64  durationMs  = 0;   ///< Duration in milliseconds.
    int     trackNumber = 0;   ///< 1-based track index within the disc.
    int     discNumber  = 0;   ///< 1-based disc number within a multi-disc release.
    QString filepath;          ///< Absolute path on the remote host.
};

/**
 * @brief REST client for a remote SoundShelf HTTP server (feature #19).
 *
 * Talks to the endpoints served by network::HttpServer:
 *  - GET /api/v1/tracks           — list or search tracks
 *  - GET /api/v1/tracks/<id>      — single-track metadata
 *  - GET /api/v1/stream/<id>      — audio stream URL (helper only, no download)
 *
 * Authentication is the bearer token sent as the @c token query parameter,
 * matching the server-side @c authorise() helper in HttpServer.cpp.
 *
 * Pure static helpers (buildListQuery, trackFromJson, parseTrackList,
 * parseTrack) have zero network dependency and are fully unit-tested without mocks.
 *
 * Blocking network methods (listTracks, track) call QFuture::result()
 * internally and must be invoked from a worker thread or the CLI
 * (never from the Qt GUI thread — that deadlocks).
 */
class RemoteClient : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs a client bound to the given server.
     *
     * @param baseUrl  Root URL, e.g. "http://192.168.1.10:8080".
     * @param token    Bearer token from @c soundshelf serve --auth.
     * @param parent   Optional QObject parent.
     */
    explicit RemoteClient(QString baseUrl, QString token, QObject* parent = nullptr);

    /**
     * @brief Builds the URL query for a /api/v1/tracks request.
     *
     * Rules:
     *  - @p query is added as @c q only when non-empty.
     *  - @p limit is added as @c limit only when > 0.
     *  - @p token is added as @c token only when non-empty.
     * Special characters in values are percent-encoded by QUrlQuery.
     */
    static QUrlQuery buildListQuery(const QString& query, int limit, const QString& token);

    /**
     * @brief Maps a JSON object from the server to a RemoteTrack.
     *
     * Mirrors HttpServer::trackToJson field-for-field:
     * @c id, @c title, @c artist, @c album, @c duration_ms,
     * @c track_no, @c disc_no, @c path.
     * Missing keys produce default values (0 / empty string).
     */
    static RemoteTrack trackFromJson(const QJsonObject& o);

    /**
     * @brief Parses a JSON array of track objects from raw server bytes.
     *
     * @param json  Raw response bytes (top-level JSON array expected).
     * @return      Track list on success; Error on non-array or parse failure.
     */
    static Result<QList<RemoteTrack>> parseTrackList(const QByteArray& json);

    /**
     * @brief Parses a single-track JSON object from raw server bytes.
     *
     * @param json  Raw response bytes (top-level JSON object expected).
     * @return      RemoteTrack on success; Error on non-object or parse failure.
     */
    static Result<RemoteTrack> parseTrack(const QByteArray& json);

    /**
     * @brief Returns the audio stream URL for @p id without downloading.
     *
     * Format: @c \<baseUrl\>/api/v1/stream/\<id\>?token=\<token\>
     * The token query parameter is omitted when the token is empty.
     */
    QString streamUrl(int id) const;

    /**
     * @brief Lists or searches tracks on the remote server (blocking).
     *
     * Must be called from a worker thread or the CLI, not the GUI thread.
     *
     * @param query  Full-text search string; empty = list all.
     * @param limit  Max results (0 = server default 100).
     * @return  Track list on success, Error::AuthenticationFailed on 401,
     *          Error::NetworkError on other failures.
     */
    Result<QList<RemoteTrack>> listTracks(const QString& query = {}, int limit = 100);

    /**
     * @brief Fetches metadata for a single track by id (blocking).
     *
     * Same threading constraints as listTracks().
     *
     * @param id  Server-side track id.
     * @return    RemoteTrack on success; Error on failure.
     */
    Result<RemoteTrack> track(int id);

private:
    QUrl       m_baseUrl;
    QString    m_token;
    RestClient m_rest;
};

} // namespace soundshelf
