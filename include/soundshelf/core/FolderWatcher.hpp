#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QSet>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Recursively watches one or more folders for new audio files.
 *
 * Wraps `QFileSystemWatcher`, which only watches the directory level —
 * we maintain our own subdirectory list and re-arm the watcher when
 * subfolders appear. Audio file additions are coalesced through a
 * 500 ms debounce timer to avoid storms when a tagger rewrites tags
 * across an album.
 *
 * Wire @ref filesAdded to @ref LibraryManager to get auto-import.
 */
class FolderWatcher : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Result of comparing two sets of audio-file paths.
     *
     * Both lists are sorted ascending so callers get deterministic ordering.
     */
    struct DirDiff {
        QStringList added;   ///< Paths present in @c current but not in @c known.
        QStringList removed; ///< Paths present in @c known but not in @c current.
    };

    explicit FolderWatcher(QObject* parent = nullptr);
    ~FolderWatcher() override;

    /// Begins watching @p root recursively. Multiple roots may be added.
    Result<void> addRoot(const QString& root);

    /// Stops watching @p root.
    void removeRoot(const QString& root);

    /// All currently watched roots.
    QStringList roots() const;

    /// Pause / resume — useful while a bulk import is running.
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    /**
     * @brief Non-recursively scan @p dir for audio files.
     *
     * Returns absolute paths of regular files whose lower-cased extension
     * is among FolderReader::audioExtensions(), sorted ascending by file
     * name.  Subdirectory contents are NOT included.
     *
     * @param dir Directory to scan (must exist).
     * @return Sorted list of absolute audio-file paths (may be empty).
     */
    static QStringList scanAudioFiles(const QString& dir);

    /**
     * @brief Compute the difference between two file-path sets.
     *
     * @param known   Previously observed set of absolute paths.
     * @param current Currently observed set of absolute paths.
     * @return DirDiff with @c added and @c removed lists, both sorted ascending.
     */
    static DirDiff diffFiles(const QSet<QString>& known,
                             const QSet<QString>& current);

signals:
    /// Emitted after the debounce window with the set of newly-added
    /// audio files.
    void filesAdded(const QStringList& paths);

    /// Emitted with files that disappeared (still on watcher, but no
    /// longer on disk).
    void filesRemoved(const QStringList& paths);

private slots:
    void onDirectoryChanged(const QString& path);
    void flushPending();

private:
    QFileSystemWatcher m_fs;
    QStringList  m_roots;
    QSet<QString> m_known;
    QStringList  m_pendingAdd;
    QStringList  m_pendingDel;
    QTimer       m_debounce;
    bool         m_paused = false;
};

} // namespace soundshelf
