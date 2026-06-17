#include "soundshelf/core/LibraryManager.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/io/TagInfo.hpp"
#include "soundshelf/io/FolderReader.hpp"

#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLib, "soundshelf.core.library")

namespace soundshelf {

LibraryManager::LibraryManager(QObject* parent) : QObject(parent) {
    auto& db = DatabaseManager::instance();
    connect(&db, &DatabaseManager::trackInserted, this, &LibraryManager::trackChanged);
    connect(&db, &DatabaseManager::trackUpdated,  this, &LibraryManager::trackChanged);
}

LibraryManager::~LibraryManager() = default;

bool LibraryManager::isBusy() const { return m_busy; }

namespace {

/// Walks @p root, returning every file whose suffix is recognised as audio.
QStringList collectAudioFiles(const QString& root) {
    // NB: audioExtensions() returns by value — bind it to a named local so
    // begin()/end() iterate the SAME container. Calling .begin() and .end() on
    // two separate temporaries yields iterators into different objects, and the
    // QSet range-ctor then computes a garbage distance → huge reserve → crash.
    const QStringList extList = FolderReader::audioExtensions();
    const QSet<QString> exts(extList.begin(), extList.end());

    QStringList out;
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        if (exts.contains(QFileInfo(p).suffix().toLower())) out << p;
    }
    return out;
}

/// Pulls tags + audio properties off disk and merges them into a Track.
Result<Track> trackFromFile(const QString& path) {
    auto tagR = TagInfo::fromFile(path);
    if (!tagR) return Result<Track>::err(tagR.error().code, tagR.error().message);
    const TagInfo& t = tagR.value();

    Track tr;
    tr.filepath  = QFileInfo(path).absoluteFilePath();
    tr.filename  = QFileInfo(path).fileName();
    tr.format    = audioFormatFromFilename(path);
    tr.mtime     = QFileInfo(path).lastModified();
    t.applyToTrack(tr);
    return Result<Track>::ok(std::move(tr));
}

} // namespace

Result<void> LibraryManager::importFolder(const QString& folderPath) {
    if (m_busy) {
        return Result<void>::err(Error::DeviceNotReady,
            QStringLiteral("Library import already running"));
    }
    if (!QFileInfo::exists(folderPath)) {
        return Result<void>::err(Error::FileNotFound,
            QStringLiteral("Folder not found: %1").arg(folderPath));
    }
    m_busy = true;

    auto* watcher = new QFutureWatcher<QPair<int,int>>(this);
    connect(watcher, &QFutureWatcher<QPair<int,int>>::finished,
            this, [this, watcher]() {
        const auto result = watcher->result();
        m_busy = false;
        emit importFinished(result.first, result.second);
        watcher->deleteLater();
    });

    const QString rootCopy = folderPath;
    auto fut = QtConcurrent::run([this, rootCopy]() -> QPair<int,int> {
        const QStringList files = collectAudioFiles(rootCopy);
        int processed = 0, errors = 0;
        const int total = files.size();
        for (int i = 0; i < total; ++i) {
            const QString& f = files[i];
            auto tr = trackFromFile(f);
            if (!tr) { ++errors; continue; }
            Track tracker = tr.value();
            // Resolve foreign keys (artist / genre / disc).
            auto& dbm = DatabaseManager::instance();
            if (!tracker.artist.isEmpty()) {
                if (auto a = dbm.ensureArtist(tracker.artist); a) tracker.artistId = a.value();
            }
            if (!tracker.albumArtist.isEmpty()) {
                if (auto a = dbm.ensureArtist(tracker.albumArtist); a) tracker.albumArtistId = a.value();
            }
            if (!tracker.genre.isEmpty()) {
                if (auto g = dbm.ensureGenre(tracker.genre); g) tracker.genreId = g.value();
            }
            if (!tracker.album.isEmpty()) {
                const int aid = tracker.albumArtistId >= 0
                    ? tracker.albumArtistId : tracker.artistId;
                if (auto d = dbm.ensureFolderDisc(tracker.album, aid); d) {
                    tracker.discId = d.value();
                }
            }
            if (auto u = dbm.upsertTrack(tracker); !u) {
                qCWarning(lcLib) << "upsert failed for" << f << ":" << u.error().message;
                ++errors;
                continue;
            }
            ++processed;
            if (total > 0 && (processed % 25 == 0 || processed == total)) {
                const int pct = qBound(0, (i * 100) / qMax(total, 1), 100);
                QMetaObject::invokeMethod(this, "importProgress", Qt::QueuedConnection,
                    Q_ARG(int, pct), Q_ARG(QString, f));
            }
        }
        return { processed, errors };
    });
    watcher->setFuture(fut);
    return Result<void>::ok();
}

Result<void> LibraryManager::rescanAll() {
    auto& dbm = DatabaseManager::instance();
    auto roots = dbm.getSetting(QStringLiteral("library_roots"));
    if (!roots) return Result<void>::err(roots.error().code, roots.error().message);

    const QStringList parts = roots.value().split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& r : parts) {
        if (auto rr = importFolder(r); !rr) {
            return rr;
        }
    }
    return Result<void>::ok();
}

Result<int> LibraryManager::addTrack(const Track& track) {
    Track copy = track;
    return DatabaseManager::instance().upsertTrack(copy);
}

Result<QList<Track>> LibraryManager::search(const QString& query, int limit) {
    return DatabaseManager::instance().searchTracks(query, limit);
}

Result<Track> LibraryManager::trackByPath(const QString& filepath) {
    return DatabaseManager::instance().getTrackByPath(filepath);
}

} // namespace soundshelf
