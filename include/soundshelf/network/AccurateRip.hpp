#pragma once

#include <QVector>
#include <QByteArray>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf::accuraterip {

/**
 * @brief Computed AccurateRip and FreeDB disc identifiers derived from a CD TOC.
 */
struct DiscIds {
    quint32 id1{0};        ///< AccurateRip ID1: sum of per-track LBA offsets + leadout
    quint32 id2{0};        ///< AccurateRip ID2: weighted sum (offset × 1-based index)
    quint32 freedbId{0};   ///< Classic FreeDB/CDDB 8-hex-digit identifier
    int     trackCount{0};
};

/**
 * @brief Per-track CRC entry from a parsed AccurateRip binary response.
 */
struct TrackCrc {
    quint8  confidence{0};  ///< Number of matching rips in the database
    quint32 crc{0};         ///< ARv1 CRC32
    quint32 crc450{0};      ///< ARv2 CRC32 (skips first/last 450 frames)
};

/**
 * @brief One pressing match from a dBAR binary response.
 */
struct PressingResult {
    DiscIds           ids;
    QVector<TrackCrc> tracks;
};

/**
 * @brief Compute AccurateRip and FreeDB disc identifiers from a CD TOC.
 *
 * Uses @p toc.entries[i].startSector as per-track LBA frame offset and
 * @c (last_entry.endSector + 1) as the leadout frame. All arithmetic wraps
 * as quint32.
 *
 * AccurateRip ID algorithm:
 * - For each track i (1-based): @c id1 += offset_i; @c id2 += (offset_i ? offset_i : 1) * i
 * - Then: @c id1 += leadout; @c id2 += leadout * (n+1)
 *
 * FreeDB/CDDB algorithm:
 * - cddb_offset = startSector + 150; seconds = cddb_offset / 75
 * - n = Σ digit_sum(trackSeconds); t = leadoutSeconds - track1Seconds
 * - freedbId = ((n % 0xff) << 24) | (t << 8) | trackCount
 *
 * @param toc  Table of contents from CDDAReader or CueParser.
 * @return DiscIds with all fields populated, or zeroed DiscIds for an empty TOC.
 */
DiscIds computeDiscIds(const Toc& toc);

/**
 * @brief Parse a concatenated dBAR binary blob fetched from AccurateRip.
 *
 * Each chunk layout:
 * - 1 byte:  trackCount (TC)
 * - 4 bytes LE: id1
 * - 4 bytes LE: id2
 * - 4 bytes LE: freedbId
 * - TC × 9 bytes: (1 byte confidence + 4 bytes LE crc + 4 bytes LE crc450)
 *
 * Total chunk size: 13 + TC × 9 bytes. Iterates until the blob is consumed.
 *
 * @param blob  Raw bytes downloaded from the dBAR endpoint.
 * @return Parsed pressing results on success.
 *         @c Error{InvalidFormat} on truncation or leftover bytes.
 *         An empty blob returns an empty vector (ok).
 */
Result<QVector<PressingResult>> parseResponse(const QByteArray& blob);

} // namespace soundshelf::accuraterip
