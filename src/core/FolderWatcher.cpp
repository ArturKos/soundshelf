#include "soundshelf/core/FolderWatcher.hpp"
#include "soundshelf/io/FolderReader.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWatch, "soundshelf.core.watcher")

namespace soundshelf {

FolderWatcher::FolderWatcher(QObject* parent) : QObject(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(500);
    connect(&m_debounce, &QTimer::timeout, this, &FolderWatcher::flushPending);
    connect(&m_fs, &QFileSystemWatcher::directoryChanged,
            this, &FolderWatcher::onDirectoryChanged);
}

FolderWatcher::~FolderWatcher() = default;

QStringList FolderWatcher::roots() const { return m_roots; }

void FolderWatcher::setPaused(bool paused) {
    m_paused = paused;
    if (!paused && (!m_pendingAdd.isEmpty() || !m_pendingDel.isEmpty())) {
        m_debounce.start();
    }
}

Result<void> FolderWatcher::addRoot(const QString& root) {
    QFileInfo fi(root);
    if (!fi.isDir()) {
        return Result<void>::err(Error::FileNotFound,
            QStringLiteral("Not a directory: %1").arg(root));
    }
    const QString absRoot = fi.absoluteFilePath();
    if (m_roots.contains(absRoot)) return Result<void>::ok();

    m_roots.append(absRoot);
    QStringList dirs;
    dirs << absRoot;
    QDirIterator it(absRoot, QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) dirs << it.next();
    m_fs.addPaths(dirs);

    QStringList initial;
    scanInto(absRoot, initial);
    for (const auto& f : initial) m_known.insert(f);

    qCInfo(lcWatch) << "Watching root" << absRoot
                    << "(" << dirs.size() << "dirs," << initial.size() << "files)";
    return Result<void>::ok();
}

void FolderWatcher::removeRoot(const QString& root) {
    const QString absRoot = QFileInfo(root).absoluteFilePath();
    m_roots.removeAll(absRoot);
    QStringList toUnwatch;
    for (const QString& d : m_fs.directories()) {
        if (d == absRoot || d.startsWith(absRoot + QLatin1Char('/'))) {
            toUnwatch << d;
        }
    }
    if (!toUnwatch.isEmpty()) m_fs.removePaths(toUnwatch);

    QSet<QString> stillKnown;
    for (const QString& f : m_known) {
        if (!f.startsWith(absRoot + QLatin1Char('/'))) stillKnown.insert(f);
    }
    m_known = stillKnown;
}

void FolderWatcher::scanInto(const QString& dir, QStringList& outFiles) {
    const QSet<QString> exts = QSet<QString>(
        FolderReader::audioExtensions().begin(),
        FolderReader::audioExtensions().end());

    QDir d(dir);
    for (const QFileInfo& fi : d.entryInfoList(
             QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (exts.contains(fi.suffix().toLower())) {
            outFiles << fi.absoluteFilePath();
        }
    }
}

void FolderWatcher::onDirectoryChanged(const QString& path) {
    // Pick up new subdirectories.
    QDir d(path);
    for (const QFileInfo& fi : d.entryInfoList(
             QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString p = fi.absoluteFilePath();
        if (!m_fs.directories().contains(p)) m_fs.addPath(p);
    }

    QStringList currentFiles;
    scanInto(path, currentFiles);
    QSet<QString> currentSet(currentFiles.begin(), currentFiles.end());

    // What's known but no longer here?
    for (const QString& f : m_known) {
        if (!QFileInfo(f).absolutePath().startsWith(path)) continue;
        if (!currentSet.contains(f)) m_pendingDel << f;
    }
    // What's new?
    for (const QString& f : currentSet) {
        if (!m_known.contains(f)) m_pendingAdd << f;
    }

    // Update the known set immediately so a subsequent event doesn't
    // double-count the same file.
    for (const QString& f : m_pendingAdd) m_known.insert(f);
    for (const QString& f : m_pendingDel) m_known.remove(f);

    if (!m_paused) m_debounce.start();
}

void FolderWatcher::flushPending() {
    if (m_paused) return;
    if (!m_pendingAdd.isEmpty()) {
        qCInfo(lcWatch) << "Added" << m_pendingAdd.size() << "files";
        emit filesAdded(m_pendingAdd);
        m_pendingAdd.clear();
    }
    if (!m_pendingDel.isEmpty()) {
        qCInfo(lcWatch) << "Removed" << m_pendingDel.size() << "files";
        emit filesRemoved(m_pendingDel);
        m_pendingDel.clear();
    }
}

} // namespace soundshelf
