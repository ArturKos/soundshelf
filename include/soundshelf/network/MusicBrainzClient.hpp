#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/**
 * @brief Client for MusicBrainz Web Service v2.
 *
 * Host: `https://musicbrainz.org/ws/2`. Per the MusicBrainz politeness
 * rules anonymous clients must stay at 1 request/second; the
 * underlying @ref RestClient is rate-limited accordingly.
 *
 * The endpoints exposed here are the ones the application actually
 * uses: disc-ID lookup, release-by-MBID, and free-text release
 * search.
 */
class MusicBrainzClient : public QObject {
    Q_OBJECT
public:
    explicit MusicBrainzClient(QObject* parent = nullptr);
    ~MusicBrainzClient() override;

    /// Disc-ID lookup. The response carries release(s), tracks and
    /// artist credits.
    QFuture<Result<QJsonDocument>> lookupDiscId(const QString& discId);

    /// Release lookup with full inc set (artists + recordings + media).
    QFuture<Result<QJsonDocument>> lookupRelease(const QString& releaseMbid);

    /// Release search by artist + album.
    QFuture<Result<QJsonDocument>> searchRelease(const QString& artist,
                                                 const QString& album,
                                                 int limit = 25);

private:
    RestClient m_rest;
};

} // namespace soundshelf
