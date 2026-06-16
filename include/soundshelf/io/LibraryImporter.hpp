#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonDocument>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Imports a track catalog from a soundshelf-library JSON document.
 *
 * Validates the envelope (format string, version check against
 * LibraryExporter::FORMAT_VERSION) then deserialises each track object back
 * into a Track struct. DB-local identity fields (id, discId, artistId, …) are
 * left at their default values (−1). Optional fields (ReplayGain, cue offsets,
 * QDateTime columns) are populated only when the corresponding JSON key is
 * present in the document.
 *
 * @see LibraryExporter
 */
class LibraryImporter : public QObject {
    Q_OBJECT
public:
    explicit LibraryImporter(QObject* parent = nullptr);
    ~LibraryImporter() override;

    /**
     * @brief Deserialises a QJsonDocument produced by LibraryExporter::toJson().
     *
     * Validates that @p doc is an object with @c format=="soundshelf-library"
     * and @c version<=LibraryExporter::FORMAT_VERSION.
     *
     * @param doc Source JSON document.
     * @return Result containing the parsed track list on success;
     *         Error::InvalidArgument if the document is not an object,
     *         if the format string does not match, or if the version is
     *         unsupported (zero or greater than FORMAT_VERSION).
     */
    static Result<QList<Track>> fromJson(const QJsonDocument& doc);

    /**
     * @brief Reads a file from @p path and deserialises it.
     *
     * @param path Source file path.
     * @return Result containing the parsed track list on success;
     *         Error::FileNotFound if the path does not exist;
     *         Error::FileAccessDenied if the file cannot be opened;
     *         Error::InvalidArgument on JSON parse failure or invalid envelope.
     */
    Result<QList<Track>> importFromFile(const QString& path);
};

} // namespace soundshelf
