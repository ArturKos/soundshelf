#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/SchemaMigrator.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDb, "soundshelf.db")

namespace soundshelf {

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager mgr;
    return mgr;
}

QString DatabaseManager::defaultDbPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("library.db"));
}

bool DatabaseManager::isOpen() const {
    return m_db.isOpen();
}

QSqlDatabase DatabaseManager::database() {
    return m_db;
}

Result<void> DatabaseManager::open(const QString& dbPath) {
    if (m_db.isOpen()) m_db.close();

    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        return Result<void>::err(Error::DatabaseError,
            QStringLiteral("Cannot open DB %1: %2").arg(dbPath, m_db.lastError().text()));
    }

    // Pragmas
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    q.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    q.exec(QStringLiteral("PRAGMA encoding = 'UTF-8'"));

    qCInfo(lcDb) << "Opened database:" << dbPath;

    // Apply migrations
    SchemaMigrator migrator(m_db);
    auto mr = migrator.migrate();
    if (!mr) return mr;

    return Result<void>::ok();
}

void DatabaseManager::close() {
    if (m_db.isOpen()) m_db.close();
}

Result<int> DatabaseManager::ensureArtist(const QString& name) {
    if (name.isEmpty()) return Result<int>::ok(-1);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM artists WHERE name = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next()) return Result<int>::ok(q.value(0).toInt());

    q.prepare(QStringLiteral("INSERT INTO artists(name) VALUES (?)"));
    q.addBindValue(name);
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            QStringLiteral("Insert artist: %1").arg(q.lastError().text()));
    }
    return Result<int>::ok(q.lastInsertId().toInt());
}

Result<int> DatabaseManager::ensureGenre(const QString& name) {
    if (name.isEmpty()) return Result<int>::ok(-1);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM genres WHERE name = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next()) return Result<int>::ok(q.value(0).toInt());

    q.prepare(QStringLiteral("INSERT INTO genres(name) VALUES (?)"));
    q.addBindValue(name);
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            QStringLiteral("Insert genre: %1").arg(q.lastError().text()));
    }
    return Result<int>::ok(q.lastInsertId().toInt());
}

Result<int> DatabaseManager::upsertTrack(Track& track) {
    // Resolve FK if not yet set
    if (track.artistId < 0 && !track.artist.isEmpty()) {
        auto r = ensureArtist(track.artist);
        if (!r) return r;
        track.artistId = r.value();
    }
    if (track.genreId < 0 && !track.genre.isEmpty()) {
        auto r = ensureGenre(track.genre);
        if (!r) return r;
        track.genreId = r.value();
    }
    if (track.albumArtistId < 0 && !track.albumArtist.isEmpty()) {
        auto r = ensureArtist(track.albumArtist);
        if (!r) return r;
        track.albumArtistId = r.value();
    }
    if (track.discId < 0 && !track.album.isEmpty()) {
        const int aid = track.albumArtistId >= 0 ? track.albumArtistId : track.artistId;
        auto r = ensureFolderDisc(track.album, aid);
        if (r) track.discId = r.value();
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO tracks(filepath, filename, disc_id, artist_id, album_artist_id, "
        "genre_id, title, track_number, disc_number, year, duration_ms, bitrate, "
        "samplerate, channels, format, md5_hash, rg_track_gain, rg_track_peak, "
        "rg_album_gain, rg_album_peak, acoustid, mb_recording_id, mtime) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(filepath) DO UPDATE SET "
        "title=excluded.title, artist_id=excluded.artist_id, "
        "album_artist_id=excluded.album_artist_id, genre_id=excluded.genre_id, "
        "track_number=excluded.track_number, disc_number=excluded.disc_number, "
        "year=excluded.year, duration_ms=excluded.duration_ms, "
        "bitrate=excluded.bitrate, samplerate=excluded.samplerate, "
        "channels=excluded.channels, format=excluded.format, "
        "rg_track_gain=excluded.rg_track_gain, rg_track_peak=excluded.rg_track_peak, "
        "rg_album_gain=excluded.rg_album_gain, rg_album_peak=excluded.rg_album_peak, "
        "acoustid=excluded.acoustid, mb_recording_id=excluded.mb_recording_id, "
        "mtime=excluded.mtime, missing=0"));

    auto bindOpt = [&q](const std::optional<double>& o) {
        if (o.has_value()) q.addBindValue(*o);
        else                q.addBindValue(QVariant());
    };

    q.addBindValue(track.filepath);
    q.addBindValue(track.filename);
    q.addBindValue(track.discId >= 0 ? QVariant(track.discId) : QVariant());
    q.addBindValue(track.artistId >= 0 ? QVariant(track.artistId) : QVariant());
    q.addBindValue(track.albumArtistId >= 0 ? QVariant(track.albumArtistId) : QVariant());
    q.addBindValue(track.genreId >= 0 ? QVariant(track.genreId) : QVariant());
    q.addBindValue(track.title);
    q.addBindValue(track.trackNumber);
    q.addBindValue(track.discNumber);
    q.addBindValue(track.year);
    q.addBindValue(track.durationMs);
    q.addBindValue(track.bitrate);
    q.addBindValue(track.samplerate);
    q.addBindValue(track.channels);
    q.addBindValue(audioFormatToString(track.format));
    q.addBindValue(track.md5Hash);
    bindOpt(track.rgTrackGain);
    bindOpt(track.rgTrackPeak);
    bindOpt(track.rgAlbumGain);
    bindOpt(track.rgAlbumPeak);
    q.addBindValue(track.acoustid);
    q.addBindValue(track.mbRecordingId);
    q.addBindValue(track.mtime);

    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            QStringLiteral("Upsert track: %1").arg(q.lastError().text()));
    }

    const int id = (q.numRowsAffected() > 0 && track.id < 0)
        ? q.lastInsertId().toInt()
        : track.id;
    track.id = id;
    emit trackInserted(id);
    return Result<int>::ok(id);
}

Result<Track> DatabaseManager::getTrack(int id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT t.id, t.filepath, t.filename, t.disc_id, t.artist_id, t.album_artist_id, "
        "t.genre_id, t.title, t.track_number, t.disc_number, t.year, t.duration_ms, "
        "t.bitrate, t.samplerate, t.channels, t.format, t.md5_hash, "
        "t.rg_track_gain, t.rg_track_peak, t.rg_album_gain, t.rg_album_peak, "
        "t.acoustid, t.mb_recording_id, t.play_count, t.skip_count, t.rating, "
        "t.comment, t.missing, t.added_at, t.mtime, t.last_played, "
        "a.name AS artist_name, d.title AS disc_title, g.name AS genre_name "
        "FROM tracks t "
        "LEFT JOIN artists a ON a.id = t.artist_id "
        "LEFT JOIN discs d ON d.id = t.disc_id "
        "LEFT JOIN genres g ON g.id = t.genre_id "
        "WHERE t.id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        return Result<Track>::err(Error::DatabaseError,
            QStringLiteral("Track %1 not found").arg(id));
    }

    Track t;
    t.id = q.value(0).toInt();
    t.filepath = q.value(1).toString();
    t.filename = q.value(2).toString();
    t.discId = q.value(3).toInt();
    t.artistId = q.value(4).toInt();
    t.albumArtistId = q.value(5).toInt();
    t.genreId = q.value(6).toInt();
    t.title = q.value(7).toString();
    t.trackNumber = q.value(8).toInt();
    t.discNumber = q.value(9).toInt();
    t.year = q.value(10).toInt();
    t.durationMs = q.value(11).toInt();
    t.bitrate = q.value(12).toInt();
    t.samplerate = q.value(13).toInt();
    t.channels = q.value(14).toInt();
    t.format = audioFormatFromString(q.value(15).toString());
    t.md5Hash = q.value(16).toString();
    if (!q.value(17).isNull()) t.rgTrackGain = q.value(17).toDouble();
    if (!q.value(18).isNull()) t.rgTrackPeak = q.value(18).toDouble();
    if (!q.value(19).isNull()) t.rgAlbumGain = q.value(19).toDouble();
    if (!q.value(20).isNull()) t.rgAlbumPeak = q.value(20).toDouble();
    t.acoustid = q.value(21).toString();
    t.mbRecordingId = q.value(22).toString();
    t.playCount = q.value(23).toInt();
    t.skipCount = q.value(24).toInt();
    t.rating = q.value(25).toDouble();
    t.comment = q.value(26).toString();
    t.missing = q.value(27).toBool();
    t.addedAt = q.value(28).toDateTime();
    t.mtime = q.value(29).toDateTime();
    t.lastPlayed = q.value(30).toDateTime();
    t.artist = q.value(31).toString();
    t.album = q.value(32).toString();
    t.genre = q.value(33).toString();
    return Result<Track>::ok(t);
}

Result<Track> DatabaseManager::getTrackByPath(const QString& filepath) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM tracks WHERE filepath = ?"));
    q.addBindValue(filepath);
    if (!q.exec() || !q.next()) {
        return Result<Track>::err(Error::DatabaseError, QStringLiteral("Not found"));
    }
    return getTrack(q.value(0).toInt());
}

Result<QList<Track>> DatabaseManager::searchTracks(const QString& query, int limit) {
    QSqlQuery q(m_db);
    if (query.isEmpty()) {
        return listTracks(limit, 0);
    }
    q.prepare(QStringLiteral(
        "SELECT t.id FROM tracks_fts f "
        "JOIN tracks t ON t.id = f.rowid "
        "WHERE tracks_fts MATCH ? AND t.missing = 0 "
        "ORDER BY rank LIMIT ?"));
    q.addBindValue(query + QLatin1String("*"));
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<Track>>::err(Error::DatabaseError,
            QStringLiteral("FTS query: %1").arg(q.lastError().text()));
    }
    QList<Track> out;
    while (q.next()) {
        auto r = getTrack(q.value(0).toInt());
        if (r) out << r.value();
    }
    return Result<QList<Track>>::ok(out);
}

Result<QList<Track>> DatabaseManager::listTracks(int limit, int offset) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id FROM tracks WHERE missing = 0 "
        "ORDER BY added_at DESC LIMIT ? OFFSET ?"));
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (!q.exec()) {
        return Result<QList<Track>>::err(Error::DatabaseError,
            q.lastError().text());
    }
    QList<Track> out;
    while (q.next()) {
        auto r = getTrack(q.value(0).toInt());
        if (r) out << r.value();
    }
    return Result<QList<Track>>::ok(out);
}

Result<void> DatabaseManager::updatePlayCount(int trackId) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE tracks SET play_count = play_count + 1, "
        "last_played = CURRENT_TIMESTAMP WHERE id = ?"));
    q.addBindValue(trackId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    return Result<void>::ok();
}

Result<int> DatabaseManager::ensureFolderDisc(const QString& albumTitle, int artistId) {
    if (albumTitle.isEmpty()) return Result<int>::ok(-1);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id FROM discs WHERE title = ? AND COALESCE(artist_id, -1) = ? "
        "AND type = 'folder' LIMIT 1"));
    q.addBindValue(albumTitle);
    q.addBindValue(artistId >= 0 ? artistId : -1);
    if (q.exec() && q.next()) return Result<int>::ok(q.value(0).toInt());

    q.prepare(QStringLiteral(
        "INSERT INTO discs(title, artist_id, type) VALUES (?, ?, 'folder')"));
    q.addBindValue(albumTitle);
    q.addBindValue(artistId >= 0 ? QVariant(artistId) : QVariant());
    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError,
            QStringLiteral("ensureFolderDisc insert: %1").arg(q.lastError().text()));
    }
    return Result<int>::ok(q.lastInsertId().toInt());
}

Result<QList<Track>> DatabaseManager::tracksByDisc(int discId) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id FROM tracks WHERE disc_id = ? AND missing = 0 "
        "ORDER BY disc_number ASC, track_number ASC"));
    q.addBindValue(discId);
    if (!q.exec()) {
        return Result<QList<Track>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Track> out;
    while (q.next()) {
        if (auto r = getTrack(q.value(0).toInt()); r) out << r.value();
    }
    return Result<QList<Track>>::ok(std::move(out));
}

Result<void> DatabaseManager::markMissing(int trackId, bool missing) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE tracks SET missing = ? WHERE id = ?"));
    q.addBindValue(missing ? 1 : 0);
    q.addBindValue(trackId);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    return Result<void>::ok();
}

Result<int> DatabaseManager::upsertDisc(Disc& disc) {
    if (disc.artistId < 0 && !disc.artist.isEmpty()) {
        auto r = ensureArtist(disc.artist);
        if (!r) return r;
        disc.artistId = r.value();
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO discs(title, artist_id, year, type, source_path, toc_discid, "
        "mb_release_id, total_duration_ms, track_count) "
        "VALUES (?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(toc_discid) DO UPDATE SET "
        "title=excluded.title, artist_id=excluded.artist_id, year=excluded.year, "
        "source_path=excluded.source_path, mb_release_id=excluded.mb_release_id, "
        "total_duration_ms=excluded.total_duration_ms, "
        "track_count=excluded.track_count"));
    q.addBindValue(disc.title);
    q.addBindValue(disc.artistId >= 0 ? QVariant(disc.artistId) : QVariant());
    q.addBindValue(disc.year);
    q.addBindValue(discTypeToString(disc.type));
    q.addBindValue(disc.sourcePath);
    q.addBindValue(disc.tocDiscId.isEmpty() ? QVariant() : QVariant(disc.tocDiscId));
    q.addBindValue(disc.mbReleaseId);
    q.addBindValue(disc.totalDurationMs);
    q.addBindValue(disc.trackCount);

    if (!q.exec()) {
        return Result<int>::err(Error::DatabaseError, q.lastError().text());
    }
    const int id = q.lastInsertId().toInt();
    disc.id = id;
    emit discInserted(id);
    return Result<int>::ok(id);
}

Result<Disc> DatabaseManager::getDisc(int id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT d.id, d.title, d.artist_id, d.year, d.type, d.source_path, "
        "d.toc_discid, d.mb_release_id, d.total_duration_ms, d.track_count, "
        "d.added_at, a.name "
        "FROM discs d LEFT JOIN artists a ON a.id = d.artist_id "
        "WHERE d.id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        return Result<Disc>::err(Error::DatabaseError,
            QStringLiteral("Disc %1 not found").arg(id));
    }
    Disc d;
    d.id = q.value(0).toInt();
    d.title = q.value(1).toString();
    d.artistId = q.value(2).toInt();
    d.year = q.value(3).toInt();
    d.type = discTypeFromString(q.value(4).toString());
    d.sourcePath = q.value(5).toString();
    d.tocDiscId = q.value(6).toString();
    d.mbReleaseId = q.value(7).toString();
    d.totalDurationMs = q.value(8).toInt();
    d.trackCount = q.value(9).toInt();
    d.addedAt = q.value(10).toDateTime();
    d.artist = q.value(11).toString();
    return Result<Disc>::ok(d);
}

Result<Disc> DatabaseManager::getDiscByDiscId(const QString& tocDiscId) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM discs WHERE toc_discid = ?"));
    q.addBindValue(tocDiscId);
    if (!q.exec() || !q.next()) {
        return Result<Disc>::err(Error::DatabaseError, QStringLiteral("Not found"));
    }
    return getDisc(q.value(0).toInt());
}

Result<QList<Disc>> DatabaseManager::searchDiscs(const QString& query, int limit) {
    QSqlQuery q(m_db);
    if (query.isEmpty()) {
        return listDiscs(DiscType::Folder, limit);  // arbitralnie, NOTE poniżej
    }
    q.prepare(QStringLiteral(
        "SELECT d.id FROM discs_fts f JOIN discs d ON d.id = f.rowid "
        "WHERE discs_fts MATCH ? ORDER BY rank LIMIT ?"));
    q.addBindValue(query + QLatin1String("*"));
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<Disc>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Disc> out;
    while (q.next()) {
        auto r = getDisc(q.value(0).toInt());
        if (r) out << r.value();
    }
    return Result<QList<Disc>>::ok(out);
}

Result<QList<Disc>> DatabaseManager::listDiscs(DiscType filter, int limit) {
    QSqlQuery q(m_db);
    Q_UNUSED(filter);  // TODO: dodać WHERE type=? gdy != "any"
    q.prepare(QStringLiteral(
        "SELECT id FROM discs ORDER BY added_at DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<Disc>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<Disc> out;
    while (q.next()) {
        auto r = getDisc(q.value(0).toInt());
        if (r) out << r.value();
    }
    return Result<QList<Disc>>::ok(out);
}

Result<QString> DatabaseManager::getSetting(const QString& key) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next()) {
        return Result<QString>::ok({});  // brak = pusty string
    }
    return Result<QString>::ok(q.value(0).toString());
}

Result<void> DatabaseManager::setSetting(const QString& key, const QString& value) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec()) {
        return Result<void>::err(Error::DatabaseError, q.lastError().text());
    }
    return Result<void>::ok();
}

} // namespace soundshelf
