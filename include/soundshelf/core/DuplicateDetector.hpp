#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Finds duplicate tracks using a layered strategy.
 *
 * The detector chains three increasingly tolerant comparisons:
 *  1. **byteHash**  — identical files (MD5 / SHA1 of the full byte stream).
 *  2. **acoustid**  — same fingerprint via AcoustID — survives transcodes.
 *  3. **tagMatch**  — `(artist, album, title, durationMs/2000)` collision.
 *
 * Each pass operates on the previous pass' "no-match" residue, so a
 * file that's both byte-identical and AcoustID-identical only appears
 * once in the result.
 *
 * Returns groups of `Track` rows (one group = one duplicate set with
 * 2+ members). Use @ref Strategy as a bitmask to enable subset of the
 * three passes.
 */
class DuplicateDetector : public QObject {
    Q_OBJECT
public:
    enum Strategy {
        ByByteHash = 1 << 0,
        ByAcoustId = 1 << 1,
        ByTags     = 1 << 2,
        All        = ByByteHash | ByAcoustId | ByTags,
    };
    Q_DECLARE_FLAGS(Strategies, Strategy)

    /// One detected duplicate group.
    struct Group {
        Strategy reason = ByByteHash;
        QList<Track> tracks;
    };

    explicit DuplicateDetector(QObject* parent = nullptr);
    ~DuplicateDetector() override;

    /// Scans the entire library and returns duplicate groups.
    Result<QList<Group>> findDuplicates(Strategies strategies = All);

    /// Test helpers — run a single pass over an in-memory list.
    static QList<Group> groupByByteHash(const QList<Track>& tracks);
    static QList<Group> groupByAcoustId(const QList<Track>& tracks);
    static QList<Group> groupByTags(const QList<Track>& tracks);

signals:
    /// Progress 0–100 + currently inspected file.
    void progress(int pct, const QString& currentPath);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DuplicateDetector::Strategies)

} // namespace soundshelf
