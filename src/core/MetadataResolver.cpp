#include "soundshelf/core/MetadataResolver.hpp"

#include "soundshelf/core/ChromaprintEngine.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/io/TagInfo.hpp"
#include "soundshelf/network/AcoustIDClient.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSqlQuery>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcMetadata, "soundshelf.core.metadata")

namespace soundshelf {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

bool isPlaceholder(const QString& s) {
    if (s.isEmpty()) return true;
    const QString l = s.toLower();
    return l == QLatin1String("unknown")
        || l == QLatin1String("unknown artist")
        || l == QLatin1String("unknown album");
}

} // namespace

// ---------------------------------------------------------------------------
// Pure static helpers
// ---------------------------------------------------------------------------

bool MetadataResolver::hasMissingTags(const Track& t) {
    return isPlaceholder(t.title) || isPlaceholder(t.artist) || isPlaceholder(t.album);
}

std::optional<ParsedName> MetadataResolver::parseFromFilename(const QString& filepath) {
    const QFileInfo fi(filepath);
    QString base = fi.completeBaseName();
    base.replace(QLatin1Char('_'), QLatin1Char(' '));
    base = base.trimmed();

    if (base.isEmpty()) return std::nullopt;

    ParsedName p;

    // Parent-folder → album (optionally "Artist - Album")
    const QString folderRaw = fi.dir().dirName();
    QString folder = folderRaw;
    folder.replace(QLatin1Char('_'), QLatin1Char(' '));
    folder = folder.trimmed();
    if (!folder.isEmpty() && folder != QLatin1String(".")) {
        static const QRegularExpression reFolderArtistAlbum(
            QStringLiteral(R"(^(.+?)\s*-\s*(.+)$)"));
        const auto fm = reFolderArtistAlbum.match(folder);
        if (fm.hasMatch()) {
            p.artist = fm.captured(1).trimmed();
            p.album  = fm.captured(2).trimmed();
        } else {
            p.album = folder;
        }
    }

    // --- Pattern 1: "NN - Artist - Title" ---
    // Three segments separated by " - "; first is 1-3 digits.
    static const QRegularExpression re1(
        QStringLiteral(R"(^(\d{1,3})\s*-\s*(.+?)\s*-\s*(.+)$)"));
    const auto m1 = re1.match(base);
    if (m1.hasMatch()) {
        p.trackNumber = m1.captured(1).toInt();
        p.artist      = m1.captured(2).trimmed();
        p.title       = m1.captured(3).trimmed();
        return p;
    }

    // --- Pattern 2: "NN - Title" ---
    // Two segments; first is 1-3 digits.
    static const QRegularExpression re2(
        QStringLiteral(R"(^(\d{1,3})\s*-\s*(.+)$)"));
    const auto m2 = re2.match(base);
    if (m2.hasMatch()) {
        p.trackNumber = m2.captured(1).toInt();
        p.title       = m2.captured(2).trimmed();
        return p;
    }

    // --- Pattern 3: "Artist - Title" ---
    // Two segments; first must NOT be purely 1-3 digits (to avoid re-matching
    // track numbers that pattern 2 should have caught).
    static const QRegularExpression re3(
        QStringLiteral(R"(^(.+?)\s*-\s*(.+)$)"));
    static const QRegularExpression reAllDigits(QStringLiteral(R"(^\d{1,3}$)"));
    const auto m3 = re3.match(base);
    if (m3.hasMatch()) {
        const QString maybeArtist = m3.captured(1).trimmed();
        if (!reAllDigits.match(maybeArtist).hasMatch()) {
            p.artist = maybeArtist;
            p.title  = m3.captured(2).trimmed();
            return p;
        }
    }

    // --- Pattern 4: "NN. Title" ---
    static const QRegularExpression re4(
        QStringLiteral(R"(^(\d{1,3})\.\s*(.+)$)"));
    const auto m4 = re4.match(base);
    if (m4.hasMatch()) {
        p.trackNumber = m4.captured(1).toInt();
        p.title       = m4.captured(2).trimmed();
        return p;
    }

    // --- Pattern 5: "NN Title" ---
    static const QRegularExpression re5(
        QStringLiteral(R"(^(\d{1,3})\s+(.+)$)"));
    const auto m5 = re5.match(base);
    if (m5.hasMatch()) {
        p.trackNumber = m5.captured(1).toInt();
        p.title       = m5.captured(2).trimmed();
        return p;
    }

    // No pattern matched; still return if at least album was derived from folder.
    if (!p.album.isEmpty()) return p;
    return std::nullopt;
}

bool MetadataResolver::fillFromParsed(Track& t, const ParsedName& p) {
    bool changed = false;

    if (!p.title.isEmpty() && isPlaceholder(t.title)) {
        t.title = p.title;
        changed = true;
    }
    if (!p.artist.isEmpty() && isPlaceholder(t.artist)) {
        t.artist = p.artist;
        changed = true;
    }
    if (!p.album.isEmpty() && isPlaceholder(t.album)) {
        t.album = p.album;
        changed = true;
    }
    if (p.trackNumber > 0 && t.trackNumber == 0) {
        t.trackNumber = p.trackNumber;
        changed = true;
    }
    return changed;
}

// ---------------------------------------------------------------------------
// Instance methods
// ---------------------------------------------------------------------------

Result<QList<int>> MetadataResolver::candidateIds(DatabaseManager& db, bool missingOnly) {
    auto r = db.listTracks(1'000'000);
    if (!r) return r.error();

    QList<int> ids;
    ids.reserve(r.value().size());
    for (const auto& t : r.value()) {
        if (!missingOnly || hasMissingTags(t))
            ids.append(t.id);
    }
    return ids;
}

Result<int> MetadataResolver::syncMissing(DatabaseManager& db, const QList<int>& trackIds) {
    // Fetch AcoustID key from DB settings (may be empty).
    QString apiKey;
    if (auto k = db.getSetting(QStringLiteral("acoustid.api_key")); k && !k.value().isEmpty())
        apiKey = k.value();

    const bool networkEnabled = !apiKey.isEmpty() && ChromaprintEngine::isAvailable();

    int updated = 0;

    for (const int id : trackIds) {
        auto trackRes = db.getTrack(id);
        if (!trackRes) {
            qCDebug(lcMetadata) << "getTrack failed for id" << id << ":" << trackRes.error().message;
            continue;
        }
        Track track = trackRes.value();

        if (!hasMissingTags(track))
            continue;

        // Normalize FK IDs: getTrack converts NULL SQL values to 0 via toInt(),
        // but upsertTrack only skips FK resolution for IDs < 0 (its sentinel for
        // "not yet persisted").  Setting 0-valued IDs to -1 prevents passing a
        // non-existent row ID 0 as a FK reference on the second upsert.
        if (track.artistId      == 0) track.artistId      = -1;
        if (track.albumArtistId == 0) track.albumArtistId = -1;
        if (track.genreId       == 0) track.genreId       = -1;
        if (track.discId        == 0) track.discId        = -1;

        // --- Step 1: Network resolution (best-effort) ---
        ParsedName networkParsed;
        bool networkOk = false;

        if (networkEnabled) {
            ChromaprintEngine cp;
            auto fpRes = cp.fingerprintFile(track.filepath);
            if (fpRes) {
                AcoustIDClient client;
                client.setApiKey(apiKey);

                QFutureWatcher<Result<QJsonDocument>> watcher;
                QEventLoop loop;
                QObject::connect(&watcher, &QFutureWatcherBase::finished,
                                 &loop, &QEventLoop::quit);
                watcher.setFuture(client.lookup(fpRes.value().fingerprint,
                                                fpRes.value().durationSec));
                loop.exec();

                const auto lookupRes = watcher.result();
                if (lookupRes) {
                    const auto results = lookupRes.value().object()
                                            .value(QStringLiteral("results")).toArray();
                    if (!results.isEmpty()) {
                        const auto first = results.first().toObject();
                        const auto recs  = first.value(QStringLiteral("recordings")).toArray();
                        if (!recs.isEmpty()) {
                            const auto rec = recs.first().toObject();
                            networkParsed.title = rec.value(QStringLiteral("title")).toString();
                            const auto artists = rec.value(QStringLiteral("artists")).toArray();
                            if (!artists.isEmpty())
                                networkParsed.artist = artists.first().toObject()
                                                           .value(QStringLiteral("name")).toString();
                            const auto rgs = rec.value(QStringLiteral("releasegroups")).toArray();
                            if (!rgs.isEmpty())
                                networkParsed.album = rgs.first().toObject()
                                                          .value(QStringLiteral("title")).toString();
                            networkOk = !networkParsed.title.isEmpty();
                        }
                    }
                } else {
                    qCDebug(lcMetadata) << "AcoustID lookup failed for" << track.filepath
                                        << ":" << lookupRes.error().message;
                }
            } else {
                qCDebug(lcMetadata) << "Fingerprint failed for" << track.filepath
                                    << ":" << fpRes.error().message;
            }
        }

        // --- Step 2: Filename fallback ---
        ParsedName parsed = networkOk ? networkParsed
                                       : parseFromFilename(track.filepath).value_or(ParsedName{});

        // --- Step 3: Fill missing fields ---
        // Remember which FK-backed fields are currently placeholder so we can
        // reset their IDs after filling.  getTrack returns NULL IDs as 0, but
        // upsertTrack only re-resolves them when the ID is < 0.
        const bool artistWasMissing = isPlaceholder(track.artist);
        const bool albumWasMissing  = isPlaceholder(track.album);

        if (!fillFromParsed(track, parsed))
            continue;

        // Reset FK IDs for any field that was just filled so upsertTrack picks
        // up the new name and creates/finds the correct FK row.
        if (artistWasMissing && !track.artist.isEmpty()) track.artistId = -1;
        if (albumWasMissing  && !track.album.isEmpty())  { track.discId = -1; track.artistId = -1; }

        // --- Step 4: Write tags to file (best-effort; failure is non-fatal) ---
        auto tagRes = TagInfo::fromFile(track.filepath);
        if (tagRes) {
            auto tags = tagRes.value();  // mutable copy
            if (!track.title.isEmpty()  && tags.title.isEmpty())  tags.title  = track.title;
            if (!track.artist.isEmpty() && tags.artist.isEmpty()) tags.artist = track.artist;
            if (!track.album.isEmpty()  && tags.album.isEmpty())  tags.album  = track.album;
            if (track.trackNumber > 0   && tags.trackNumber == 0) tags.trackNumber = track.trackNumber;
            const auto saveRes = tags.saveTo(track.filepath);
            if (!saveRes)
                qCDebug(lcMetadata) << "Tag write failed for" << track.filepath
                                    << ":" << saveRes.error().message;
        } else {
            qCDebug(lcMetadata) << "TagInfo::fromFile failed for" << track.filepath
                                << "(file may not be a real audio file)";
        }

        // --- Step 5: Persist to DB ---
        auto upsertRes = db.upsertTrack(track);
        if (!upsertRes) {
            qCDebug(lcMetadata) << "DB upsert failed for track" << id
                                << ":" << upsertRes.error().message;
            continue;
        }

        // upsertTrack's ON CONFLICT UPDATE SET deliberately omits disc_id
        // (design decision: disc association is set on first import only).
        // When we just resolved a disc for a track that previously had none,
        // patch disc_id with a direct UPDATE so getTrack returns the album.
        if (albumWasMissing && track.discId > 0) {
            QSqlQuery dq(db.database());
            dq.prepare(QStringLiteral("UPDATE tracks SET disc_id = ? WHERE id = ?"));
            dq.addBindValue(track.discId);
            dq.addBindValue(track.id);
            if (!dq.exec())
                qCDebug(lcMetadata) << "disc_id patch failed for track" << track.id;
        }

        ++updated;
        qCDebug(lcMetadata) << "Updated track" << id << track.filepath;
    }

    return updated;
}

} // namespace soundshelf
