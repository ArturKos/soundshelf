#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QByteArray>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/**
 * @brief Client for the Cover Art Archive (`https://coverartarchive.org`).
 *
 * Returns the front cover image for a MusicBrainz release MBID. The
 * archive serves redirects to `archive.org`-hosted JPEGs; the
 * underlying @ref RestClient follows redirects automatically.
 *
 * Convention: 250-pixel preview, 500-pixel medium, full-size original.
 */
class CoverArtClient : public QObject {
    Q_OBJECT
public:
    enum class Size { Preview250, Medium500, Original };

    explicit CoverArtClient(QObject* parent = nullptr);
    ~CoverArtClient() override;

    /// Fetches the front cover for @p releaseMbid at the given @p size.
    QFuture<Result<QByteArray>> fetchFront(const QString& releaseMbid,
                                           Size size = Size::Medium500);

private:
    RestClient m_rest;
};

} // namespace soundshelf
