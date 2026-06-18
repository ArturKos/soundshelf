#pragma once

#include <QString>
#include <QList>
#include <optional>
#include <functional>
#include "soundshelf/core/Track.hpp"
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

class DatabaseManager;

/**
 * @brief Filename-derived tag fields produced by @ref MetadataResolver::parseFromFilename.
 *
 * Only fields that could be parsed are populated; an empty string means the
 * pattern did not yield a value.
 */
struct ParsedName {
    QString artist;          ///< Derived from filename or parent-folder name (may be empty)
    QString title;           ///< Derived from filename basename (may be empty)
    QString album;           ///< Derived from parent-folder name (may be empty)
    int     trackNumber = 0; ///< Leading track number extracted from basename (0 if absent)
};

/**
 * @brief Fills missing or placeholder metadata for tracks in the library.
 *
 * Resolution strategy per track (in priority order):
 *   1. Network lookup via AcoustID + MusicBrainz, gated on
 *      `acoustid.api_key` being set and @ref ChromaprintEngine availability.
 *   2. Derivation from the filename / parent-folder name via
 *      @ref parseFromFilename.
 *
 * Tag changes are written back to the audio file via @ref TagInfo and then
 * persisted to the database.  Network and file-I/O failures for individual
 * tracks are logged and skipped; they do not abort the batch.
 *
 * The three pure static helpers (@ref hasMissingTags, @ref parseFromFilename,
 * @ref fillFromParsed) can be exercised in unit tests without a database or
 * network connection.
 */
class MetadataResolver {
public:
    MetadataResolver() = default;

    // -----------------------------------------------------------------------
    // Pure helpers — no I/O, safe to call from tests
    // -----------------------------------------------------------------------

    /**
     * @brief Returns @c true when the track has at least one missing/placeholder field.
     *
     * A field is considered missing when it is empty or (case-insensitively)
     * equals @c "Unknown", @c "Unknown Artist", or @c "Unknown Album".
     * The fields checked are @c title, @c artist, and @c album.
     */
    static bool hasMissingTags(const Track& t);

    /**
     * @brief Derives @ref ParsedName fields from a filesystem path.
     *
     * Examines the basename (extension stripped) and the parent-folder name.
     * Underscores are treated as spaces.  Leading/trailing whitespace is trimmed.
     *
     * Supported basename patterns (applied in priority order):
     *   - @c "NN - Artist - Title"  → trackNumber, artist, title
     *   - @c "NN - Title"           → trackNumber, title  (NN = 1–3 digits)
     *   - @c "Artist - Title"       → artist, title       (first part not all-digits)
     *   - @c "NN. Title"            → trackNumber, title
     *   - @c "NN Title"             → trackNumber, title
     *
     * The parent-folder name is mapped to @c album.  If it matches
     * @c "Artist - Album", the artist component is also propagated (subject
     * to @ref fillFromParsed only using it when the track artist is still
     * empty).
     *
     * @return @c std::nullopt when neither a title nor an album could be derived.
     */
    static std::optional<ParsedName> parseFromFilename(const QString& filepath);

    /**
     * @brief Copies non-empty fields from @p p into @p t, respecting existing values.
     *
     * A field in @p t is only overwritten when it is considered missing by
     * @ref hasMissingTags logic (empty or a well-known placeholder string).
     * @c trackNumber in @p t is filled only when @c t.trackNumber == 0 and
     * @c p.trackNumber > 0.
     *
     * @return @c true if at least one field of @p t was changed.
     */
    static bool fillFromParsed(Track& t, const ParsedName& p);

    // -----------------------------------------------------------------------
    // Instance methods — require a database
    // -----------------------------------------------------------------------

    /**
     * @brief Collects track IDs that are candidates for a metadata sync run.
     *
     * @param db          Open database to query.
     * @param missingOnly When @c true only tracks for which @ref hasMissingTags
     *                    returns @c true are included.  When @c false all track
     *                    IDs are returned.
     * @return Sorted list of IDs, or an @ref Error on database failure.
     */
    Result<QList<int>> candidateIds(DatabaseManager& db, bool missingOnly = true);

    /**
     * @brief Fills missing metadata for each ID in @p trackIds and persists changes.
     *
     * For every track ID:
     *   1. Loads the track from @p db.
     *   2. Skips when @ref hasMissingTags returns @c false.
     *   3. Attempts network resolution (AcoustID → MusicBrainz) when both
     *      @c acoustid.api_key is stored in the database settings and
     *      @ref ChromaprintEngine::isAvailable() returns @c true.
     *      Any network failure silently falls through to step 4.
     *   4. Calls @ref parseFromFilename and @ref fillFromParsed.
     *   5. When @ref fillFromParsed changed the track: attempts to write the
     *      updated tags to the file via @ref TagInfo (failure is logged and
     *      skipped, not fatal), then persists the updated @ref Track via
     *      @p db.upsertTrack.
     *
     * @param db        Open database; must outlive this call.
     * @param trackIds  List of track IDs to process.
     * @return Number of tracks that were actually updated, or an @ref Error
     *         only on pre-flight database failures (individual track errors
     *         are logged and skipped).
     */
    Result<int> syncMissing(DatabaseManager& db, const QList<int>& trackIds);
};

} // namespace soundshelf
