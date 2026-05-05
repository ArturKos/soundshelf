#include "soundshelf/core/DiscEnricher.hpp"
#include "soundshelf/core/Disc.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/network/MusicBrainzClient.hpp"
#include "soundshelf/network/CoverArtClient.hpp"

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcEnrich, "soundshelf.disc.enrich")

namespace soundshelf {

DiscEnricher::DiscEnricher(MusicBrainzClient* mb,
                           CoverArtClient* coverArt,
                           QObject* parent)
    : QObject(parent), m_mb(mb), m_coverArt(coverArt)
{}

DiscEnricher::~DiscEnricher() = default;

void DiscEnricher::enrichByDiscId(int localDiscId) {
    auto r = DatabaseManager::instance().getDisc(localDiscId);
    if (!r) {
        emit enrichmentFinished(localDiscId, false,
            QStringLiteral("Disc %1 not in DB").arg(localDiscId));
        return;
    }
    enrichByTocId(localDiscId, r.value().tocDiscId);
}

void DiscEnricher::enrichByTocId(int localDiscId, const QString& tocDiscId) {
    if (!m_mb) {
        emit enrichmentFinished(localDiscId, false,
            QStringLiteral("No MusicBrainz client"));
        return;
    }
    if (tocDiscId.isEmpty()) {
        emit enrichmentFinished(localDiscId, false,
            QStringLiteral("Disc has no tocDiscId — physical CDs only"));
        return;
    }
    qCInfo(lcEnrich) << "Looking up MB disc id" << tocDiscId
                     << "for local disc" << localDiscId;

    auto fut = m_mb->lookupDiscId(tocDiscId);
    auto* watcher = new QFutureWatcher<Result<QJsonDocument>>(this);
    connect(watcher, &QFutureWatcher<Result<QJsonDocument>>::finished,
            this, [this, watcher, localDiscId]() {
        onLookupResult(localDiscId, watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(fut);
}

void DiscEnricher::onLookupResult(int localDiscId,
                                  const Result<QJsonDocument>& res) {
    if (!res) {
        qCWarning(lcEnrich) << "MB lookup failed:" << res.error().message;
        emit enrichmentFinished(localDiscId, false, res.error().message);
        return;
    }
    const QJsonObject root = res.value().object();
    const QJsonArray releases = root.value(QStringLiteral("releases")).toArray();
    if (releases.isEmpty()) {
        emit enrichmentFinished(localDiscId, false,
            QStringLiteral("No releases for disc"));
        return;
    }

    // Pick the first release (could be smarter — first English-language
    // pressing, etc., but MB sorts by score so [0] is usually right).
    const QJsonObject rel = releases.first().toObject();
    const QString releaseMbid = rel.value(QStringLiteral("id")).toString();
    const QString title       = rel.value(QStringLiteral("title")).toString();
    const QString date        = rel.value(QStringLiteral("date")).toString();  // "YYYY-..."
    const QString barcode     = rel.value(QStringLiteral("barcode")).toString();
    QString artist;
    if (auto credits = rel.value(QStringLiteral("artist-credit")).toArray();
            !credits.isEmpty()) {
        artist = credits.first().toObject()
                    .value(QStringLiteral("name")).toString();
    }
    QString label, catalogNo;
    if (auto labels = rel.value(QStringLiteral("label-info")).toArray();
            !labels.isEmpty()) {
        const auto li = labels.first().toObject();
        label = li.value(QStringLiteral("label")).toObject()
                  .value(QStringLiteral("name")).toString();
        catalogNo = li.value(QStringLiteral("catalog-number")).toString();
    }

    // Update the disc row.
    auto& dbm = DatabaseManager::instance();
    auto db = dbm.database();
    int artistId = -1;
    if (!artist.isEmpty()) {
        if (auto a = dbm.ensureArtist(artist); a) artistId = a.value();
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE discs SET title = COALESCE(NULLIF(?, ''), title), "
        "artist_id = COALESCE(?, artist_id), "
        "year = COALESCE(NULLIF(?, 0), year), "
        "mb_release_id = ?, "
        "label = COALESCE(NULLIF(?, ''), label), "
        "catalog_no = COALESCE(NULLIF(?, ''), catalog_no), "
        "barcode = COALESCE(NULLIF(?, ''), barcode) "
        "WHERE id = ?"));
    q.addBindValue(title);
    q.addBindValue(artistId >= 0 ? QVariant(artistId) : QVariant());
    q.addBindValue(date.left(4).toInt());
    q.addBindValue(releaseMbid);
    q.addBindValue(label);
    q.addBindValue(catalogNo);
    q.addBindValue(barcode);
    q.addBindValue(localDiscId);
    if (!q.exec()) {
        qCWarning(lcEnrich) << "Disc update failed:" << q.lastError().text();
    }

    qCInfo(lcEnrich) << "Updated disc" << localDiscId
                     << "→" << artist << "/" << title
                     << "(" << releaseMbid << ")";

    // Optional cover lookup.
    if (m_fetchCover && m_coverArt && !releaseMbid.isEmpty()) {
        fetchCoverFor(localDiscId, releaseMbid);
    } else {
        emit enrichmentFinished(localDiscId, true, title);
    }
}

void DiscEnricher::fetchCoverFor(int localDiscId, const QString& releaseMbid) {
    auto fut = m_coverArt->fetchFront(releaseMbid,
        CoverArtClient::Size::Medium500);
    auto* watcher = new QFutureWatcher<Result<QByteArray>>(this);
    connect(watcher, &QFutureWatcher<Result<QByteArray>>::finished,
            this, [this, watcher, localDiscId]() {
        const auto r = watcher->result();
        watcher->deleteLater();
        if (!r) {
            // 404 from CAA is normal — many releases have no cover.
            qCDebug(lcEnrich) << "No cover for disc" << localDiscId
                              << ":" << r.error().message;
            emit enrichmentFinished(localDiscId, true, QString());
            return;
        }
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral(
            "UPDATE discs SET cover_data = ? WHERE id = ? "
            "AND (cover_data IS NULL OR length(cover_data) = 0)"));
        q.addBindValue(r.value());
        q.addBindValue(localDiscId);
        q.exec();
        qCInfo(lcEnrich) << "Cover fetched for disc" << localDiscId
                         << r.value().size() << "bytes";
        emit enrichmentFinished(localDiscId, true, QString());
    });
    watcher->setFuture(fut);
}

} // namespace soundshelf
