#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <optional>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Exports a list of tracks as M3U / PLS / XSPF playlists.
 *
 * Paths are written relative to the playlist file by default — this
 * keeps playlists portable when the music tree is moved as a whole.
 * Pass @c relativePaths=false to force absolute paths instead.
 */
class PlaylistExporter : public QObject {
    Q_OBJECT
public:
    enum class Format { M3U, PLS, XSPF };

    explicit PlaylistExporter(QObject* parent = nullptr);
    ~PlaylistExporter() override;

    /// Writes @p tracks to @p path. Format inferred from @p path extension
    /// unless @p forceFormat is supplied.
    Result<void> exportToFile(const QString& path,
                              const QList<Track>& tracks,
                              bool relativePaths = true,
                              std::optional<Format> forceFormat = std::nullopt);

    /// Format inferred from file extension. Defaults to @ref Format::M3U.
    static Format formatFromExtension(const QString& path);

    /// Pure-string serialisers (no file IO) — useful for tests and for
    /// the headless server that streams playlists over HTTP.
    static QString toM3U (const QList<Track>& tracks, const QString& baseDir, bool relative);
    static QString toPLS (const QList<Track>& tracks, const QString& baseDir, bool relative);
    static QString toXSPF(const QList<Track>& tracks, const QString& baseDir, bool relative);
};

} // namespace soundshelf
