#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonDocument>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Exports the full track catalog as a portable JSON document.
 *
 * Serialises all portable, non-DB-local fields (filepath, tags, ReplayGain,
 * AcoustID, statistics, optional cue offsets) into a self-describing JSON
 * envelope. DB-internal identity columns (id, discId, artistId, …) and binary
 * blobs (coverHash, coverData) are deliberately omitted — they are re-resolved
 * from the denormalised string fields by LibraryManager on re-import.
 *
 * Envelope structure:
 * @code
 * {
 *   "format": "soundshelf-library",
 *   "version": 1,
 *   "exported_at": "<ISO 8601 UTC>",
 *   "track_count": N,
 *   "tracks": [ { … }, … ]
 * }
 * @endcode
 *
 * Optional fields (ReplayGain doubles, cue offsets) are omitted from the
 * per-track object when std::nullopt. QDateTime fields are serialised as
 * ISO 8601 UTC strings and omitted when the value is null/invalid.
 *
 * @see LibraryImporter
 */
class LibraryExporter : public QObject {
    Q_OBJECT
public:
    /// JSON envelope format version written by this class.
    static constexpr int FORMAT_VERSION = 1;

    explicit LibraryExporter(QObject* parent = nullptr);
    ~LibraryExporter() override;

    /**
     * @brief Serialises @p tracks into a QJsonDocument.
     *
     * Pure in-memory operation — no file I/O. The returned document contains
     * the full envelope including the "tracks" array.
     *
     * @param tracks Source track list (may be empty).
     * @return QJsonDocument with the complete library envelope.
     */
    static QJsonDocument toJson(const QList<Track>& tracks);

    /**
     * @brief Writes @p tracks as indented JSON to @p path.
     *
     * Creates or truncates the destination file atomically via QSaveFile.
     *
     * @param path   Destination file path.
     * @param tracks Track list to export (may be empty).
     * @return Result<void>::ok() on success;
     *         Error::FileAccessDenied if the file cannot be opened or committed.
     */
    Result<void> exportToFile(const QString& path, const QList<Track>& tracks);
};

} // namespace soundshelf
