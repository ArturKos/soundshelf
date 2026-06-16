#pragma once

#include <QString>
#include <QList>
#include <QMap>

namespace soundshelf {

/**
 * @brief One timed line from an LRC document.
 *
 * @a text is already stripped of all timestamp tags;
 * @a timeMs already has the document's @c offset applied.
 */
struct LrcLine {
    int timeMs = 0;  ///< Absolute playback position in milliseconds.
    QString text;    ///< Lyric text; may be empty for instrumental gaps.
};

/**
 * @brief Parsed representation of an LRC (Lyric) file.
 *
 * @ref lines are sorted ascending by @ref LrcLine::timeMs.
 * The @ref offsetMs value from the `[offset:±ms]` tag has already been
 * subtracted from every entry's @c timeMs (negative results clamped to 0).
 */
struct LrcDocument {
    QList<LrcLine> lines;             ///< All timed lines, sorted ascending by timeMs.
    QMap<QString, QString> metadata;  ///< ID tags (ar, ti, al, au, length, by, re, ve) — lowercased keys.
    int offsetMs = 0;                 ///< Raw value of the [offset:±ms] tag before application.

    /// Returns @c true when the document contains at least one timed line.
    bool hasTimedLines() const { return !lines.isEmpty(); }
};

/**
 * @brief Stateless LRC (Lyric) file parser (I/O layer, Qt Core only).
 *
 * Supported features:
 *  - Multiple timestamp tags on one input line: `[mm:ss.cc][mm:ss.cc]text`
 *    produces two @ref LrcLine entries sharing the same text.
 *  - Fraction-digit handling:
 *    - 1 digit  → tenths of a second (×100 ms)
 *    - 2 digits → centiseconds       (×10  ms)
 *    - 3 digits → milliseconds       (×1   ms)
 *  - ID/metadata tags (`[ar:]`, `[ti:]`, `[al:]`, `[offset:]`, …) are
 *    stored in @ref LrcDocument::metadata and not emitted as timed lines.
 *  - `[offset:±ms]` is applied to every line's timestamp (timeMs = rawMs − offset),
 *    with negative results clamped to 0.
 *  - Blank text after timestamp stripping is preserved (instrumental gaps).
 *  - Lines with no valid timestamp and not a metadata tag are silently ignored.
 *
 * Logging category: `soundshelf.io.lrc`.
 */
class LrcParser {
public:
    /**
     * @brief Parses LRC text into an @ref LrcDocument.
     *
     * Malformed input degrades gracefully: unparseable lines are skipped
     * and the returned document may be partially populated.
     *
     * @param lrc  Full LRC file content as a string.
     * @return     Parsed document with @ref LrcDocument::lines sorted by timeMs.
     */
    static LrcDocument parse(const QString& lrc);

    /**
     * @brief Returns the index of the active lyric line for a playback position.
     *
     * Performs a linear scan over the (already sorted) document and returns
     * the index of the last @ref LrcLine whose @c timeMs ≤ @a positionMs.
     *
     * @param doc         Parsed LRC document (lines must be sorted ascending).
     * @param positionMs  Current playback position in milliseconds.
     * @return            0-based index, or -1 when @a positionMs precedes the first line.
     */
    static int lineIndexForMs(const LrcDocument& doc, int positionMs);
};

} // namespace soundshelf
