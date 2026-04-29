#pragma once

#include <QString>
#include <QDateTime>
#include <QByteArray>
#include <QList>
#include "Track.hpp"

namespace soundshelf {

enum class DiscType {
    Physical,    ///< Fizyczne CD-DA w napędzie
    Folder,      ///< Katalog z plikami
    Image,       ///< Obraz CUE/BIN/ISO
    Remote       ///< Zdalna biblioteka
};

QString discTypeToString(DiscType t);
DiscType discTypeFromString(const QString& s);

struct Disc {
    int id = -1;
    QString title;
    int artistId = -1;
    QString artist;        ///< denormalised
    int year = 0;
    DiscType type = DiscType::Folder;
    QString sourcePath;    ///< /dev/sr0 lub ~/Music/X lub *.cue lub URL

    // External IDs
    QString tocDiscId;     ///< MusicBrainz disc ID
    QString mbReleaseId;
    QString freedbId;
    QString accuraterRipId;

    int totalDurationMs = 0;
    int trackCount = 0;
    QByteArray coverData;

    QString label;
    QString catalogNo;
    QString barcode;

    QDateTime addedAt;

    QList<Track> tracks;   ///< Wypełnia DiscManager::loadTracks()

    bool isValid() const { return id >= 0 && !title.isEmpty(); }
};

} // namespace soundshelf
