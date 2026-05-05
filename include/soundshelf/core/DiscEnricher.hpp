#pragma once

#include <QObject>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

class MusicBrainzClient;
class CoverArtClient;
struct Disc;

/**
 * @brief Async metadata enrichment for @ref Disc rows.
 *
 * Given a disc that already has its `tocDiscId` (computed by
 * @ref CDDAReader::computeDiscId) the enricher:
 *
 *  1. Queries @ref MusicBrainzClient::lookupDiscId for the matching
 *     release.
 *  2. Picks the first release in the response and updates the disc:
 *     `mb_release_id`, `title`, `artist_id` (resolved from the credit),
 *     `year`, `label`, `catalog_no`, `barcode`.
 *  3. If cover art is missing, calls @ref CoverArtClient::fetchFront
 *     with the release MBID and writes the bytes to `cover_data`.
 *
 * Pure async — emits @ref enrichmentFinished when done. The host may
 * react by reloading the disc grid.
 *
 * Designed to be called from the UI thread; the actual HTTP work
 * happens on @ref MusicBrainzClient's QNAM.
 */
class DiscEnricher : public QObject {
    Q_OBJECT
public:
    DiscEnricher(MusicBrainzClient* mb, CoverArtClient* coverArt,
                 QObject* parent = nullptr);
    ~DiscEnricher() override;

    /// Pulls the disc by id from the DB, runs the lookup chain, and
    /// emits @ref enrichmentFinished. No-op (still emits) if the disc
    /// has no `tocDiscId`.
    void enrichByDiscId(int localDiscId);

    /// Same as above but takes a `tocDiscId` directly — useful when
    /// the caller already has it in hand and just wants metadata.
    void enrichByTocId(int localDiscId, const QString& tocDiscId);

    /// Skip cover-art lookup (useful in tests / for users behind
    /// metered connections).
    void setFetchCoverArt(bool on) { m_fetchCover = on; }
    bool fetchCoverArt() const { return m_fetchCover; }

signals:
    /// Emitted exactly once per @ref enrichBy* call. @p ok is false
    /// when MusicBrainz returned no match or the request failed.
    void enrichmentFinished(int localDiscId, bool ok, const QString& message);

private:
    void onLookupResult(int localDiscId, const Result<class QJsonDocument>& res);
    void fetchCoverFor(int localDiscId, const QString& releaseMbid);

    MusicBrainzClient* m_mb = nullptr;
    CoverArtClient*    m_coverArt = nullptr;
    bool               m_fetchCover = true;
};

} // namespace soundshelf
