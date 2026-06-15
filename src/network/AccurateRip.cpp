#include "soundshelf/network/AccurateRip.hpp"

#include <QDataStream>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAR, "soundshelf.network.accuraterip")

namespace soundshelf::accuraterip {

namespace {

/// Sum of decimal digits of @p n (e.g. 135 → 1+3+5 = 9). Returns 0 for n≤0.
int cddbDigitSum(int n) {
    int s = 0;
    while (n > 0) {
        s += n % 10;
        n /= 10;
    }
    return s;
}

} // namespace

DiscIds computeDiscIds(const Toc& toc) {
    const int n = toc.entries.size();
    if (n == 0)
        return DiscIds{};

    const long leadout = toc.entries.last().endSector + 1;

    quint32 id1 = 0;
    quint32 id2 = 0;

    for (int i = 0; i < n; ++i) {
        const quint32 offset = quint32(toc.entries[i].startSector);
        id1 += offset;
        id2 += quint32(offset ? offset : 1) * quint32(i + 1);
    }
    id1 += quint32(leadout);
    id2 += quint32(leadout) * quint32(n + 1);

    // FreeDB/CDDB id
    const int leadoutSeconds = (int(leadout) + 150) / 75;
    const int track1Seconds  = (int(toc.entries[0].startSector) + 150) / 75;

    int digitSum = 0;
    for (int i = 0; i < n; ++i) {
        const int seconds = (int(toc.entries[i].startSector) + 150) / 75;
        digitSum += cddbDigitSum(seconds);
    }

    const int t = leadoutSeconds - track1Seconds;
    const quint32 freedbId = (quint32(digitSum % 0xff) << 24)
                           | (quint32(t) << 8)
                           | quint32(n);

    qCDebug(lcAR) << "computeDiscIds: tracks=" << n
                  << "leadout=" << leadout
                  << Qt::hex
                  << "id1=0x" << id1
                  << "id2=0x" << id2
                  << "freedbId=0x" << freedbId;

    return DiscIds{id1, id2, freedbId, n};
}

Result<QVector<PressingResult>> parseResponse(const QByteArray& blob) {
    if (blob.isEmpty())
        return QVector<PressingResult>{};

    QVector<PressingResult> results;
    QDataStream ds(blob);
    ds.setByteOrder(QDataStream::LittleEndian);

    while (!ds.atEnd()) {
        quint8  tc;
        quint32 cid1, cid2, cfdb;
        ds >> tc >> cid1 >> cid2 >> cfdb;
        if (ds.status() != QDataStream::Ok)
            return Result<QVector<PressingResult>>::err(
                Error::InvalidFormat,
                QStringLiteral("AccurateRip: truncated chunk header"));

        PressingResult pr;
        pr.ids = DiscIds{cid1, cid2, cfdb, int(tc)};
        pr.tracks.reserve(tc);

        for (quint8 i = 0; i < tc; ++i) {
            TrackCrc entry;
            quint32  crc, crc450;
            ds >> entry.confidence >> crc >> crc450;
            if (ds.status() != QDataStream::Ok)
                return Result<QVector<PressingResult>>::err(
                    Error::InvalidFormat,
                    QStringLiteral("AccurateRip: truncated track entry %1 of %2")
                        .arg(i + 1).arg(tc));
            entry.crc    = crc;
            entry.crc450 = crc450;
            pr.tracks.append(entry);
        }
        results.append(std::move(pr));
    }

    qCDebug(lcAR) << "parseResponse: parsed" << results.size() << "pressing(s)";
    return results;
}

} // namespace soundshelf::accuraterip
