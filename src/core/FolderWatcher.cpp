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

    const QStringList initial = scanAudioFiles(absRoot);
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

QStringList FolderWatcher::scanAudioFiles(const QString& dir) {
    const QStringList extList = FolderReader::audioExtensions();
    const QSet<QString> exts(extList.begin(), extList.end());
    QDir d(dir);
    QStringList result;
    for (const QFileInfo& fi : d.entryInfoList(
             QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (exts.contains(fi.suffix().toLower()))
            result << fi.absoluteFilePath();
    }
    return result;
}

FolderWatcher::DirDiff FolderWatcher::diffFiles(const QSet<QString>& known,
                                                const QSet<QString>& current) {
    DirDiff diff;
    for (const QString& f : current) {
        if (!known.contains(f)) diff.added << f;
    }
    for (const QString& f : known) {
        if (!current.contains(f)) diff.removed << f;
    }
    diff.added.sort();
    diff.removed.sort();
    return diff;
}

void FolderWatcher::onDirectoryChanged(const QString& path) {
    // Pick up new subdirectories.
    QDir d(path);
    for (const QFileInfo& fi : d.entryInfoList(
             QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString p = fi.absoluteFilePath();
        if (!m_fs.directories().contains(p)) m_fs.addPath(p);
    }

    const QStringList currentList = scanAudioFiles(path);
    const QSet<QString> currentSet(currentList.begin(), currentList.end());

    // Build the subset of m_known that belongs to this directory.
    QSet<QString> knownSubset;
    for (const QString& f : m_known) {
        if (QFileInfo(f).absolutePath().startsWith(path))
            knownSubset.insert(f);
    }

    const DirDiff diff = diffFiles(knownSubset, currentSet);
    m_pendingAdd << diff.added;
    m_pendingDel << diff.removed;

    // Update the known set immediately so a subsequent event doesn't
    // double-count the same file.
    for (const QString& f : diff.added)   m_known.insert(f);
    for (const QString& f : diff.removed) m_known.remove(f);

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
