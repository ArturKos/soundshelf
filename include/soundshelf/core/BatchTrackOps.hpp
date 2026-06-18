#pragma once

#include <QList>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

class DatabaseManager;

/**
 * @brief Stateless helpers for batch operations on a set of tracks.
 *
 * All methods operate on plain data and Qt file I/O — no QObject, no
 * signals. Designed to be called from MainWindow (or CLI) after the UI
 * collects the checked track set.
 *
 * Error handling follows the project-wide @ref Result<T> contract;
 * no exceptions are thrown.
 *
 * Logging category: @c soundshelf.core.batchops
 */
namespace BatchTrackOps {

/**
 * @brief Copy each track's file into @p destDir using its original filename.
 *
 * @p destDir is created if it does not yet exist.
 *
 * On filename collision the destination file is skipped (not overwritten);
 * the returned count reflects only files actually written.
 *
 * @param tracks   Tracks to copy (filepath must be non-empty).
 * @param destDir  Absolute path of the destination folder.
 * @return Number of files successfully copied, or Err on I/O failure.
 */
Result<int> copyToFolder(const QList<Track>& tracks, const QString& destDir);

/**
 * @brief Remove track rows from the database; leave files on disk.
 *
 * Delegates to @ref DatabaseManager::removeTracks in a single transaction.
 *
 * @param db        Open @ref DatabaseManager instance.
 * @param trackIds  DB ids to delete.
 * @return Number of rows removed, or Err on database failure.
 */
Result<int> removeFromLibrary(DatabaseManager& db, const QList<int>& trackIds);

/**
 * @brief Delete files from disk AND remove the corresponding DB rows.
 *
 * For each id the file path is looked up via @ref DatabaseManager::getTrack,
 * the file is removed with @c QFile::remove, then the DB row is deleted.
 * If the file cannot be removed (e.g. access denied) that track is skipped
 * and the count reflects only successfully deleted pairs.
 *
 * The DB rows of successfully deleted files are removed in a single
 * transaction after all file deletions have been attempted.
 *
 * @param db        Open @ref DatabaseManager instance.
 * @param trackIds  DB ids to process.
 * @return Number of (file + DB row) pairs removed, or Err on DB failure.
 */
Result<int> deleteFiles(DatabaseManager& db, const QList<int>& trackIds);

} // namespace BatchTrackOps
} // namespace soundshelf
