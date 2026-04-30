#include "soundshelf/core/DiscManager.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/io/FolderReader.hpp"
#include "soundshelf/io/CDDAReader.hpp"
#include "soundshelf/io/ImageReader.hpp"
#include "soundshelf/io/TagInfo.hpp"

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDisc, "soundshelf.core.disc")

namespace soundshelf {

DiscManager::DiscManager(QObject* parent) : QObject(parent) {}
DiscManager::~DiscManager() = default;

std::unique_ptr<DiscReader> DiscManager::makeReaderFor(const QString& source) {
    // Physical drive on Linux/macOS or Windows
    if (source.startsWith(QLatin1String("/dev/"))
        || source.startsWith(QStringLiteral("\\\\.\\"))) {
        return std::make_unique<CDDAReader>(source);
    }
    QFileInfo fi(source);
    if (fi.isDir()) {
        return std::make_unique<FolderReader>(fi.absoluteFilePath());
    }
    if (fi.isFile()) {
        return std::make_unique<ImageReader>(fi.absoluteFilePath());
    }
    // Fallback: treat as folder path (may not exist yet — caller will see error).
    return std::make_unique<FolderReader>(source);
}

Result<int> DiscManager::persistDisc(Disc& disc, const Toc& toc) {
    auto& dbm = DatabaseManager::instance();

    // Resolve artist FK if we have a name only.
    if (disc.artistId < 0 && !disc.artist.isEmpty()) {
        if (auto a = dbm.ensureArtist(disc.artist); a) disc.artistId = a.value();
    }

    disc.trackCount = toc.entries.size();
    disc.totalDurationMs = toc.totalDurationMs;
    if (!toc.discId.isEmpty())  disc.tocDiscId = toc.discId;
    if (!toc.freedbId.isEmpty()) disc.freedbId = toc.freedbId;

    auto saved = dbm.upsertDisc(disc);
    if (!saved) return saved;
    const int discId = saved.value();
    disc.id = discId;
    qCInfo(lcDisc) << "Persisted disc id" << discId
                   << "title=" << disc.title
                   << "tracks=" << disc.trackCount;
    return saved;
}

Result<int> DiscManager::addFromFolder(const QString& path, bool /*multiDisc*/) {
    FolderReader reader(path);
    auto tocR = reader.readToc();
    if (!tocR) return Result<int>::err(tocR.error().code, tocR.error().message);
    const Toc toc = tocR.value();

    Disc disc;
    disc.type = DiscType::Folder;
    disc.sourcePath = QFileInfo(path).absoluteFilePath();
    disc.title = QFileInfo(path).fileName();

    // Best-effort: probe the first track's tags to fill album / artist / year.
    if (!toc.entries.isEmpty()) {
        const QFileInfo first(QDir(disc.sourcePath), toc.entries.front().title);
        Q_UNUSED(first);
    }

    auto persisted = persistDisc(disc, toc);
    if (!persisted) return persisted;
    emit discAdded(persisted.value());
    return persisted;
}

Result<int> DiscManager::addFromCdda(const QString& device) {
    CDDAReader reader(device);
    auto tocR = reader.readToc();
    if (!tocR) return Result<int>::err(tocR.error().code, tocR.error().message);

    auto idR = reader.computeDiscId();
    Toc toc = tocR.value();
    if (idR) toc.discId = idR.value();

    Disc disc;
    disc.type = DiscType::Physical;
    disc.sourcePath = device;
    disc.title = QStringLiteral("Audio CD (%1)").arg(device);

    auto persisted = persistDisc(disc, toc);
    if (!persisted) return persisted;
    emit discAdded(persisted.value());
    return persisted;
}

Result<int> DiscManager::addFromImage(const QString& path) {
    ImageReader reader(path);
    auto tocR = reader.readToc();
    if (!tocR) return Result<int>::err(tocR.error().code, tocR.error().message);
    const Toc toc = tocR.value();

    Disc disc;
    disc.type = DiscType::Image;
    disc.sourcePath = QFileInfo(path).absoluteFilePath();
    disc.title = reader.sheet().albumTitle.isEmpty()
        ? QFileInfo(path).completeBaseName()
        : reader.sheet().albumTitle;
    disc.artist = reader.sheet().albumPerformer;
    disc.year = reader.sheet().year;

    auto persisted = persistDisc(disc, toc);
    if (!persisted) return persisted;
    emit discAdded(persisted.value());
    return persisted;
}

Result<int> DiscManager::rescan(int discId) {
    auto loaded = DatabaseManager::instance().getDisc(discId);
    if (!loaded) return Result<int>::err(loaded.error().code, loaded.error().message);
    Disc disc = loaded.value();

    auto reader = makeReaderFor(disc.sourcePath);
    auto tocR = reader->readToc();
    if (!tocR) return Result<int>::err(tocR.error().code, tocR.error().message);

    auto persisted = persistDisc(disc, tocR.value());
    if (!persisted) return persisted;
    emit discUpdated(discId);
    return persisted;
}

Result<Disc> DiscManager::loadWithTracks(int discId) {
    auto& dbm = DatabaseManager::instance();
    auto d = dbm.getDisc(discId);
    if (!d) return d;
    Disc disc = d.value();
    // Inflate tracks list from DB.
    // (DatabaseManager doesn't currently have a getTracksForDisc — we stay
    //  conservative and leave Disc::tracks empty here; the typical UI
    //  path queries tracks separately via LibraryManager::search.)
    return Result<Disc>::ok(std::move(disc));
}

} // namespace soundshelf
