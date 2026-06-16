#include "soundshelf/io/LrcParser.hpp"

#include <QRegularExpression>
#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(lcLrc, "soundshelf.io.lrc")

namespace soundshelf {
namespace {

/// Convert a fraction string to milliseconds.
/// 1 digit → tenths (×100 ms), 2 digits → centiseconds (×10 ms), 3 digits → ms (×1).
int fractionToMs(const QString& frac) {
    if (frac.isEmpty()) return 0;
    const int val = frac.toInt();
    switch (frac.length()) {
        case 1: return val * 100;
        case 2: return val * 10;
        default: return val;  // 3+ digits treated as ms
    }
}

} // namespace

LrcDocument LrcParser::parse(const QString& lrc) {
    LrcDocument doc;
    if (lrc.isEmpty()) return doc;

    // Matches a single timestamp tag: [mm:ss] or [mm:ss.frac]
    static const QRegularExpression reTimestamp(
        QStringLiteral("\\[(\\d+):(\\d+)(?:\\.(\\d+))?\\]"));
    // Matches a whole-line metadata/ID tag: [key:value] where key starts with a letter
    static const QRegularExpression reMetadata(
        QStringLiteral("^\\[([a-zA-Z][a-zA-Z0-9]*):(.*?)\\]$"));

    struct RawLine { int rawMs; QString text; };
    QList<RawLine> rawLines;

    const QStringList inputLines = lrc.split(QLatin1Char('\n'));
    for (QString line : inputLines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;

        // Collect all timestamp matches in this line (globalMatch finds all occurrences)
        const auto allMatches = [&] {
            QList<QRegularExpressionMatch> v;
            auto it = reTimestamp.globalMatch(line);
            while (it.hasNext()) v.append(it.next());
            return v;
        }();

        // Only accept timestamps that are contiguous from position 0
        QList<int> timestamps;
        int expectedPos = 0;
        for (const auto& m : allMatches) {
            if (m.capturedStart() != expectedPos) break;
            const int min = m.captured(1).toInt();
            const int sec = m.captured(2).toInt();
            const int fms = fractionToMs(m.captured(3));
            timestamps.append((min * 60 + sec) * 1000 + fms);
            expectedPos = m.capturedEnd();
        }

        if (!timestamps.isEmpty()) {
            const QString text = line.mid(expectedPos).trimmed();
            for (int ts : timestamps)
                rawLines.append({ts, text});
        } else {
            // Check for a metadata/ID tag spanning the whole line
            const auto meta = reMetadata.match(line);
            if (meta.hasMatch()) {
                const QString key   = meta.captured(1).toLower();
                const QString value = meta.captured(2).trimmed();
                if (key == QLatin1String("offset")) {
                    doc.offsetMs = value.toInt();
                    qCDebug(lcLrc) << "LRC offset:" << doc.offsetMs << "ms";
                } else {
                    doc.metadata.insert(key, value);
                    qCDebug(lcLrc) << "LRC metadata:" << key << "=" << value;
                }
            }
            // Lines that are neither timestamps nor metadata are silently ignored
        }
    }

    // Apply offset to each raw timestamp and clamp negatives to 0
    doc.lines.reserve(rawLines.size());
    for (const auto& raw : rawLines) {
        const int timeMs = std::max(0, raw.rawMs - doc.offsetMs);
        doc.lines.append({timeMs, raw.text});
    }

    // Sort ascending by timeMs; stable_sort preserves order of equal-time entries
    std::stable_sort(doc.lines.begin(), doc.lines.end(),
        [](const LrcLine& a, const LrcLine& b) { return a.timeMs < b.timeMs; });

    qCDebug(lcLrc) << "Parsed LRC:" << doc.lines.size() << "lines, offset" << doc.offsetMs << "ms";
    return doc;
}

int LrcParser::lineIndexForMs(const LrcDocument& doc, int positionMs) {
    int best = -1;
    for (int i = 0; i < doc.lines.size(); ++i) {
        if (doc.lines[i].timeMs <= positionMs)
            best = i;
        else
            break;  // lines are sorted — no earlier line can satisfy the condition
    }
    return best;
}

} // namespace soundshelf
