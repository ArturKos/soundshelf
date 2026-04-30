#include "soundshelf/core/DuplicateDetector.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QHash>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDup, "soundshelf.core.duplicates")

namespace soundshelf {

DuplicateDetector::DuplicateDetector(QObject* parent) : QObject(parent) {}
DuplicateDetector::~DuplicateDetector() = default;

namespace {

QString tagKey(const Track& t) {
    // Round duration to nearest 2 s so that re-encodes match.
    const int durBucket = (t.durationMs / 1000 + 1) / 2;
    return QStringLiteral("%1%2%3%4")
        .arg(t.artist.toLower())
        .arg(t.album.toLower())
        .arg(t.title.toLower())
        .arg(durBucket);
}

QByteArray quickHash(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Md5);
    if (!h.addData(&f)) return {};
    return h.result();
}

} // namespace

QList<DuplicateDetector::Group>
DuplicateDetector::groupByByteHash(const QList<Track>& tracks) {
    QHash<QByteArray, QList<Track>> buckets;
    for (const auto& t : tracks) {
        if (!t.md5Hash.isEmpty()) {
            buckets[t.md5Hash.toUtf8()].append(t);
            continue;
        }
        const QByteArray h = quickHash(t.filepath);
        if (h.isEmpty()) continue;
        buckets[h].append(t);
    }
    QList<Group> out;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        if (it.value().size() < 2) continue;
        Group g; g.reason = ByByteHash; g.tracks = it.value();
        out.append(g);
    }
    return out;
}

QList<DuplicateDetector::Group>
DuplicateDetector::groupByAcoustId(const QList<Track>& tracks) {
    QHash<QString, QList<Track>> buckets;
    for (const auto& t : tracks) {
        if (t.acoustid.isEmpty()) continue;
        buckets[t.acoustid].append(t);
    }
    QList<Group> out;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        if (it.value().size() < 2) continue;
        Group g; g.reason = ByAcoustId; g.tracks = it.value();
        out.append(g);
    }
    return out;
}

QList<DuplicateDetector::Group>
DuplicateDetector::groupByTags(const QList<Track>& tracks) {
    QHash<QString, QList<Track>> buckets;
    for (const auto& t : tracks) {
        if (t.title.isEmpty() || t.durationMs <= 0) continue;
        buckets[tagKey(t)].append(t);
    }
    QList<Group> out;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        if (it.value().size() < 2) continue;
        Group g; g.reason = ByTags; g.tracks = it.value();
        out.append(g);
    }
    return out;
}

Result<QList<DuplicateDetector::Group>>
DuplicateDetector::findDuplicates(Strategies strategies) {
    auto& dbm = DatabaseManager::instance();
    auto allR = dbm.listTracks(1000000, 0);
    if (!allR) return Result<QList<Group>>::err(allR.error().code, allR.error().message);
    const QList<Track> all = allR.value();

    QList<Group> groups;
    QSet<int> claimedIds;
    auto stripClaimed = [&](const QList<Group>& src) {
        QList<Group> out;
        for (const auto& g : src) {
            QList<Track> fresh;
            for (const auto& t : g.tracks) {
                if (!claimedIds.contains(t.id)) fresh.append(t);
            }
            if (fresh.size() >= 2) {
                Group ng = g; ng.tracks = fresh;
                out.append(ng);
                for (const auto& t : ng.tracks) claimedIds.insert(t.id);
            }
        }
        return out;
    };

    if (strategies.testFlag(ByByteHash)) {
        const auto pass = groupByByteHash(all);
        groups += stripClaimed(pass);
    }
    if (strategies.testFlag(ByAcoustId)) {
        const auto pass = groupByAcoustId(all);
        groups += stripClaimed(pass);
    }
    if (strategies.testFlag(ByTags)) {
        const auto pass = groupByTags(all);
        groups += stripClaimed(pass);
    }
    qCInfo(lcDup) << "Found" << groups.size() << "duplicate groups";
    return Result<QList<Group>>::ok(std::move(groups));
}

} // namespace soundshelf
