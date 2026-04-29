#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include "soundshelf/io/CDDAReader.hpp"
#include "soundshelf/io/FormatConverter.hpp"
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief High-level orchestration of CD-DA ripping.
 *
 * Sequences the rip pipeline:
 * 1. @ref CDDAReader::readToc and @ref CDDAReader::computeDiscId
 * 2. iterate the TOC, calling @ref CDDAReader::ripTrackToWav per track
 * 3. optionally hand each WAV to @ref FormatConverter for compression
 *
 * Output filenames follow `%track%-%title%.%ext%` by default; a custom
 * pattern with the placeholders `%track%`, `%title%`, `%artist%`,
 * `%album%`, `%year%`, `%ext%` can be supplied. Disc-level metadata
 * is filled in by the caller (typically from MusicBrainz lookup).
 *
 * Emits @ref trackStarted / @ref trackFinished / @ref allFinished so
 * the UI can drive a progress dialog.
 */
class DiscRipper : public QObject {
    Q_OBJECT
public:
    /// Job specification.
    struct Job {
        QString device;             ///< /dev/sr0 or D:
        QString outputDir;
        QString filenamePattern { QStringLiteral("%track%-%title%.%ext%") };
        QString discTitle;
        QString discArtist;
        int     year = 0;
        bool    convertAfter = true;
        FormatConverter::Format targetFormat = FormatConverter::Format::Flac;
        bool    deleteWavAfterConvert = true;
    };

    explicit DiscRipper(QObject* parent = nullptr);
    ~DiscRipper() override;

    /// Starts a rip. Returns immediately; use signals to follow progress.
    Result<void> start(const Job& job);

    /// Aborts after the currently-ripping track finishes.
    void cancel();

    /// True between @ref start and @ref allFinished.
    bool isRunning() const;

    /// Substitutes pattern placeholders.
    static QString applyPattern(const QString& pattern,
                                int trackNumber,
                                const QString& title,
                                const QString& artist,
                                const QString& album,
                                int year,
                                const QString& ext);

signals:
    /// Emitted when the rip of @p trackNumber begins.
    void trackStarted(int trackNumber);

    /// Emitted when the rip + optional convert of @p trackNumber finishes.
    void trackFinished(int trackNumber, bool ok, const QString& path);

    /// Emitted exactly once, after the last track or upon cancel.
    void allFinished(bool ok);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace soundshelf
