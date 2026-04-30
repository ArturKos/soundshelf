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
    void scanInto(const QString& dir, QStringList& outFiles);

    QFileSystemWatcher m_fs;
    QStringList  m_roots;
    QSet<QString> m_known;
    QStringList  m_pendingAdd;
    QStringList  m_pendingDel;
    QTimer       m_debounce;
    bool         m_paused = false;
};

} // namespace soundshelf
