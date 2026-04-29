#pragma once

#include <QString>
#include <QList>
#include "soundshelf/core/Disc.hpp"
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Wpis Table Of Contents — jeden wpis = jedna ścieżka.
struct TocEntry {
    int trackNumber = 0;
    long startSector = 0;
    long endSector = 0;
    int durationMs = 0;
    QString title;       ///< wypełniane jeśli źródło zna metadane
};

struct Toc {
    QList<TocEntry> entries;
    int totalDurationMs = 0;
    QString discId;       ///< MusicBrainz disc ID (libdiscid SHA1)
    QString freedbId;     ///< CDDB/freedb ID (CRC32)
};

/// Abstrakcyjny interfejs dla wszystkich źródeł płyt.
/// Implementacje: CDDAReader, FolderReader, ImageReader.
class DiscReader {
public:
    virtual ~DiscReader() = default;

    /// Czyta strukturę płyty (lista ścieżek + długości).
    virtual Result<Toc> readToc() = 0;

    /// Zwraca typ — Physical / Folder / Image.
    virtual DiscType type() const = 0;

    /// Źródło — ścieżka, urządzenie, URL.
    virtual QString source() const = 0;

    /// Czy implementacja wspiera streaming bez kopiowania (bezpośrednie odtwarzanie).
    virtual bool supportsDirectPlayback() const { return true; }

    /// Czy implementacja wspiera ripowanie do plików.
    virtual bool supportsRipping() const { return false; }
};

} // namespace soundshelf
