#include "soundshelf/io/FolderReader.hpp"
#include "soundshelf/io/TagInfo.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(lcFolder, "soundshelf.disc.folder")

namespace soundshelf {

FolderReader::FolderReader(QString folderPath)
    : m_path(std::move(folderPath))
{}

QStringList FolderReader::audioExtensions() {
    return {
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("ogg"),
        QStringLiteral("oga"),
        QStringLiteral("opus"),
        QStringLiteral("aac"),
        QStringLiteral("m4a"),
        QStringLiteral("mp4"),
        QStringLiteral("wav"),
        QStringLiteral("ape"),
        QStringLiteral("wv"),
        QStringLiteral("alac"),
    };
}

Result<Toc> FolderReader::readToc() {
    QDir dir(m_path);
    if (!dir.exists()) {
        return Result<Toc>::err(Error::FileNotFound,
            QStringLiteral("Folder not found: %1").arg(m_path));
    }

    // Buduj filter list: *.mp3, *.flac, ...
    QStringList filters;
    for (const auto& ext : audioExtensions()) {
        filters << QStringLiteral("*.%1").arg(ext);
        filters << QStringLiteral("*.%1").arg(ext.toUpper());
    }
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Name);

    const QFileInfoList files = dir.entryInfoList();
    if (files.isEmpty()) {
        return Result<Toc>::err(Error::InvalidFormat,
            QStringLiteral("No audio files in %1").arg(m_path));
    }

    Toc toc;
    long fakeSector = 0;  // foldery nie mają sektorów, ale TocEntry tego wymaga
    int trackNumberFromTags = 0;

    // Czytamy tagi z każdego pliku, próbujemy uszanować TRACKNUMBER
    struct Entry {
        QFileInfo fi;
        TagInfo tag;
        bool tagOk;
    };
    QList<Entry> entries;
    entries.reserve(files.size());

    for (const auto& fi : files) {
        Entry e;
        e.fi = fi;
        auto r = TagInfo::fromFile(fi.absoluteFilePath());
        e.tagOk = r.isOk();
        if (e.tagOk) e.tag = r.value();
        entries.append(e);
    }

    // Jeśli wszystkie pliki mają sensowne TRACKNUMBER, sortuj po nim.
    // W przeciwnym razie zostaw sortowanie po nazwie.
    const bool allHaveTrackNum = std::all_of(entries.cbegin(), entries.cend(),
        [](const Entry& e) { return e.tagOk && e.tag.trackNumber > 0; });

    if (allHaveTrackNum) {
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.tag.discNumber != b.tag.discNumber) {
                return a.tag.discNumber < b.tag.discNumber;
            }
            return a.tag.trackNumber < b.tag.trackNumber;
        });
    }

    int n = 1;
    for (const auto& e : entries) {
        TocEntry te;
        te.trackNumber = (allHaveTrackNum && e.tag.trackNumber > 0)
            ? e.tag.trackNumber
            : n;
        te.startSector = fakeSector;
        te.durationMs = e.tagOk ? e.tag.durationMs : 0;
        te.endSector = fakeSector + (te.durationMs / 1000) * 75;  // 75 sektorów/s
        te.title = e.tagOk ? e.tag.title : e.fi.completeBaseName();
        toc.entries.append(te);
        toc.totalDurationMs += te.durationMs;
        fakeSector = te.endSector;
        ++n;
    }

    qCDebug(lcFolder) << "FolderReader read" << toc.entries.size()
                      << "tracks from" << m_path
                      << "(total" << toc.totalDurationMs << "ms)";

    return Result<Toc>::ok(std::move(toc));
}

} // namespace soundshelf
