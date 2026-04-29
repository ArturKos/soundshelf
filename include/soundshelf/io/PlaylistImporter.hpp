#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Imports playlists in M3U / M3U8 / PLS / XSPF formats.
 *
 * The importer returns absolute file paths, resolved against the
 * directory of the playlist file when the playlist contains relative
 * entries. Comments (`# ...`) and the `#EXTM3U` / `#EXTINF` lines are
 * recognised and used to populate optional title hints.
 *
 * Format detection is based on the file extension first, falling back
 * to a content sniff (`<?xml` → XSPF, `[playlist]` → PLS, otherwise M3U).
 */
class PlaylistImporter : public QObject {
    Q_OBJECT
public:
    /// One imported entry — usually one track.
    struct Entry {
        QString path;            ///< absolute filesystem path or URL
        QString titleHint;       ///< from EXTINF/track block, may be empty
        int durationSec = -1;    ///< -1 = unknown
    };

    enum class Format { Unknown, M3U, PLS, XSPF };

    explicit PlaylistImporter(QObject* parent = nullptr);
    ~PlaylistImporter() override;

    /// Imports a playlist file.
    Result<QList<Entry>> importFile(const QString& path);

    /// Detects format from filename + sniffed content.
    static Format detectFormat(const QString& filename, const QByteArray& head);

private:
    Result<QList<Entry>> parseM3U(const QString& text, const QString& baseDir);
    Result<QList<Entry>> parsePLS (const QString& text, const QString& baseDir);
    Result<QList<Entry>> parseXSPF(const QString& text, const QString& baseDir);
};

} // namespace soundshelf
