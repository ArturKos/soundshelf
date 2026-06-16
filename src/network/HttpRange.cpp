#include "soundshelf/network/HttpRange.hpp"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHttpRangeHelper, "soundshelf.network.http")

namespace soundshelf {

RangeResult HttpRange::parse(const QByteArray& headerValue, qint64 totalSize)
{
    if (headerValue.isEmpty())
        return { RangeStatus::None, {} };

    if (!headerValue.startsWith("bytes=")) {
        qCDebug(lcHttpRangeHelper) << "HttpRange: unsupported unit in Range header:" << headerValue;
        return { RangeStatus::Malformed, {} };
    }

    const QByteArray spec = headerValue.mid(6); // skip "bytes="

    // Multi-range (comma-separated) is not supported.
    if (spec.contains(',')) {
        qCDebug(lcHttpRangeHelper) << "HttpRange: multi-range not supported:" << headerValue;
        return { RangeStatus::Malformed, {} };
    }

    const int dashPos = spec.indexOf('-');
    if (dashPos < 0) {
        qCDebug(lcHttpRangeHelper) << "HttpRange: missing '-' in Range spec:" << headerValue;
        return { RangeStatus::Malformed, {} };
    }

    const QByteArray startStr = spec.left(dashPos);
    const QByteArray endStr   = spec.mid(dashPos + 1);

    qint64 rangeStart = 0, rangeEnd = 0;

    if (startStr.isEmpty()) {
        // Suffix-byte-range form: bytes=-N  (last N bytes)
        if (endStr.isEmpty()) {
            qCDebug(lcHttpRangeHelper) << "HttpRange: empty suffix in Range header:" << headerValue;
            return { RangeStatus::Malformed, {} };
        }
        bool ok = false;
        const qint64 suffix = endStr.toLongLong(&ok);
        if (!ok || suffix <= 0) {
            qCDebug(lcHttpRangeHelper) << "HttpRange: invalid suffix in Range header:" << headerValue;
            return { RangeStatus::Malformed, {} };
        }
        if (totalSize == 0) {
            return { RangeStatus::Unsatisfiable, {} };
        }
        rangeEnd   = totalSize - 1;
        rangeStart = (suffix >= totalSize) ? 0 : totalSize - suffix;
    } else {
        bool startOk = false;
        rangeStart = startStr.toLongLong(&startOk);
        if (!startOk || rangeStart < 0) {
            qCDebug(lcHttpRangeHelper) << "HttpRange: invalid start in Range header:" << headerValue;
            return { RangeStatus::Malformed, {} };
        }

        if (endStr.isEmpty()) {
            // Open-end form: bytes=N-
            if (totalSize == 0 || rangeStart >= totalSize) {
                return { RangeStatus::Unsatisfiable, {} };
            }
            rangeEnd = totalSize - 1;
        } else {
            // Explicit-end form: bytes=N-M
            bool endOk = false;
            const qint64 parsedEnd = endStr.toLongLong(&endOk);
            if (!endOk || parsedEnd < 0) {
                qCDebug(lcHttpRangeHelper) << "HttpRange: invalid end in Range header:" << headerValue;
                return { RangeStatus::Malformed, {} };
            }
            // RFC 7233 §2.1: invalid if last-byte-pos < first-byte-pos.
            if (rangeStart > parsedEnd) {
                qCDebug(lcHttpRangeHelper) << "HttpRange: start > end in Range header:" << headerValue;
                return { RangeStatus::Malformed, {} };
            }
            if (rangeStart >= totalSize) {
                return { RangeStatus::Unsatisfiable, {} };
            }
            rangeEnd = qMin(parsedEnd, totalSize - 1);
        }
    }

    return { RangeStatus::Satisfiable, { rangeStart, rangeEnd } };
}

QByteArray HttpRange::contentRange(const ByteRange& r, qint64 totalSize)
{
    return "bytes " + QByteArray::number(r.start) + '-'
         + QByteArray::number(r.end) + '/'
         + QByteArray::number(totalSize);
}

QByteArray HttpRange::unsatisfiedContentRange(qint64 totalSize)
{
    return "bytes */" + QByteArray::number(totalSize);
}

} // namespace soundshelf
