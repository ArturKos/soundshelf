#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Disc.hpp"
#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/**
 * @brief Centralised entry point for adding and querying @ref Disc entities.
 *
 * `DiscManager` builds a @ref DiscReader for the requested source
 * (folder, physical CD-DA, CUE image, remote URL), reads its TOC, and
 * persists the disc + its tracks via @ref DatabaseManager. Callers
 * only need to know the source path or device — the manager picks
 * the right reader.
 */
class DiscManager : public QObject {
    Q_OBJECT
public:
    explicit DiscManager(QObject* parent = nullptr);
    ~DiscManager() override;

    /// Imports the folder at @p path as one disc (or one disc per
    /// sub-folder if `multiDisc` is set). Tracks are upserted, then
    /// the @ref Disc row is inserted/updated.
    Result<int> addFromFolder(const QString& path, bool multiDisc = false);

    /// Reads the physical CD-DA in @p device and persists it.
    /// @param device   `/dev/sr0`, `\\\\.\\D:`, etc.
    Result<int> addFromCdda(const QString& device);

    /// Imports a CUE-backed image (passing either the .cue or its audio
    /// container — both are accepted).
    Result<int> addFromImage(const QString& path);

    /// Re-reads the source of an already-known disc and updates the row.
    Result<int> rescan(int discId);

    /// Convenience: returns the @ref Disc with `id == discId` plus all
    /// of its tracks ordered by trackNumber.
    Result<Disc> loadWithTracks(int discId);

signals:
    void discAdded(int discId);
    void discUpdated(int discId);
    void discRemoved(int discId);

private:
    /// Picks the right @ref DiscReader for the path/device.
    static std::unique_ptr<DiscReader> makeReaderFor(const QString& source);

    /// Saves the TOC + disc metadata atomically.
    Result<int> persistDisc(Disc& disc, const Toc& toc);
};

} // namespace soundshelf
