#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

#include "soundshelf/core/Disc.hpp"

namespace soundshelf {

/**
 * @brief Builds MusicBrainz Release Editor seeding data from a Disc.
 *
 * Implements the browser-based Release Editor Seeding protocol documented at
 * https://musicbrainz.org/doc/Development/Release_Editor_Seeding.
 * The generated URL is opened in a browser so the user can review and submit
 * the release — no OAuth, no live POST, and no credentials are required.
 *
 * All methods are static and pure (no network I/O), making them fully
 * unit-testable without stubs or mocks.
 */
class MusicBrainzSubmitter {
public:
    /**
     * @brief Builds ordered seed form fields from a Disc.
     *
     * Returns an ordered list of (key, value) pairs matching the MusicBrainz
     * Release Editor Seeding specification.  Optional fields are omitted when
     * their source data is empty or zero.  Track-level artist credits are only
     * emitted when a track's artist differs from the disc artist (various-artists
     * / compilation case).
     *
     * Field order is deterministic:
     *   name → artist_credit → date.year → barcode → labels → mediums.format → tracks → edit_note
     *
     * @param disc     The disc whose metadata to encode.
     * @param editNote Optional edit note appended as the @c edit_note field.
     * @return Ordered list of (key, value) pairs ready for URL query serialisation.
     */
    static QList<QPair<QString, QString>> buildSeedFields(const Disc& disc,
                                                          const QString& editNote = QString());

    /**
     * @brief Builds the full MusicBrainz Release Editor seeding URL.
     *
     * Constructs @c https://musicbrainz.org/release/add with all seed fields
     * from @ref buildSeedFields percent-encoded as the query string (via
     * QUrlQuery + QUrl).  Field order is deterministic so the resulting URL
     * is stable across calls.
     *
     * @param disc     The disc to seed.
     * @param editNote Optional edit note forwarded to @ref buildSeedFields.
     * @return Fully-formed, percent-encoded QUrl ready to open in a browser.
     */
    static QUrl buildSeedUrl(const Disc& disc, const QString& editNote = QString());
};

} // namespace soundshelf
