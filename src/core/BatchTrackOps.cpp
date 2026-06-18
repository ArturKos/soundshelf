#include "soundshelf/core/BatchTrackOps.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBatchOps, "soundshelf.core.batchops")

namespace soundshelf {
namespace BatchTrackOps {

Result<int> copyToFolder(const QList<Track>& tracks, const QString& destDir) {
    if (destDir.isEmpty()) {
        return Result<int>::err(Error::InvalidArgument,
            QObject::tr("Destination directory must not be empty."));
    }

    QDir dir(destDir);
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            return Result<int>::err(Error::FileAccessDenied,
                QObject::tr("Cannot create destination directory: %1").arg(destDir));
        }
    }

    int copied = 0;
    for (const Track& t : tracks) {
        if (t.filepath.isEmpty()) continue;
        const QString filename = QFileInfo(t.filepath).fileName();
        if (filename.isEmpty()) continue;
        const QString dest = dir.filePath(filename);
        if (QFile::exists(dest)) {
            qCDebug(lcBatchOps) << "copyToFolder: skip (collision)" << dest;
            continue;
        }
        if (!QFile::copy(t.filepath, dest)) {
            qCDebug(lcBatchOps) << "copyToFolder: copy failed" << t.filepath << "->" << dest;
            continue;
        }
        qCDebug(lcBatchOps) << "copyToFolder: copied" << t.filepath << "->" << dest;
        ++copied;
    }
    return Result<int>::ok(copied);
}

Result<int> removeFromLibrary(DatabaseManager& db, const QList<int>& trackIds) {
    qCDebug(lcBatchOps) << "removeFromLibrary: ids=" << trackIds.size();
    return db.removeTracks(trackIds);
}

Result<int> deleteFiles(DatabaseManager& db, const QList<int>& trackIds) {
    if (trackIds.isEmpty()) return Result<int>::ok(0);

    // Look up paths first, then delete files, then remove DB rows for
    // each file we successfully deleted.
    QList<int> deletedIds;
    for (int id : trackIds) {
        auto r = db.getTrack(id);
        if (!r) {
            qCDebug(lcBatchOps) << "deleteFiles: getTrack failed for id=" << id;
            continue;
        }
        const QString path = r.value().filepath;
        if (path.isEmpty()) {
            qCDebug(lcBatchOps) << "deleteFiles: empty path for id=" << id;
            continue;
        }
        if (!QFile::exists(path)) {
            qCDebug(lcBatchOps) << "deleteFiles: file missing, still removing DB row:" << path;
            deletedIds << id;
            continue;
        }
        if (!QFile::remove(path)) {
            qCDebug(lcBatchOps) << "deleteFiles: cannot remove file:" << path;
            continue;
        }
        qCDebug(lcBatchOps) << "deleteFiles: removed file:" << path;
        deletedIds << id;
    }

    if (deletedIds.isEmpty()) return Result<int>::ok(0);

    auto r = db.removeTracks(deletedIds);
    if (!r) return r;

    qCDebug(lcBatchOps) << "deleteFiles: removed" << r.value() << "DB rows";
    return r;
}

} // namespace BatchTrackOps
} // namespace soundshelf
