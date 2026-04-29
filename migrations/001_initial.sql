-- migrations/001_initial.sql
-- Initial schema for SoundShelf

PRAGMA encoding = 'UTF-8';
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

-- ============================================================
-- Schema versioning
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================
-- Reference tables
-- ============================================================

CREATE TABLE IF NOT EXISTS artists (
    id              INTEGER PRIMARY KEY,
    name            TEXT UNIQUE NOT NULL,
    sort_name       TEXT,
    mb_artist_id    TEXT,
    country         TEXT
);

CREATE INDEX IF NOT EXISTS idx_artists_name ON artists(name);
CREATE INDEX IF NOT EXISTS idx_artists_mbid ON artists(mb_artist_id) WHERE mb_artist_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS genres (
    id      INTEGER PRIMARY KEY,
    name    TEXT UNIQUE NOT NULL
);

-- ============================================================
-- Discs (first-class entity)
-- ============================================================

CREATE TABLE IF NOT EXISTS discs (
    id                  INTEGER PRIMARY KEY,
    title               TEXT NOT NULL,
    artist_id           INTEGER REFERENCES artists(id) ON DELETE SET NULL,
    year                INTEGER,
    type                TEXT NOT NULL CHECK(type IN ('physical','folder','image','remote')),
    source_path         TEXT,           -- /dev/sr0, ~/Music/Album, .cue path, http://
    toc_discid          TEXT,           -- MusicBrainz disc ID (for physical CDs)
    mb_release_id       TEXT,           -- MusicBrainz Release MBID
    freedb_id           TEXT,
    accuraterip_id      TEXT,
    total_duration_ms   INTEGER,
    track_count         INTEGER,
    cover_data          BLOB,
    label               TEXT,
    catalog_no          TEXT,
    barcode             TEXT,
    added_at            DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(toc_discid)
);

CREATE INDEX IF NOT EXISTS idx_discs_artist ON discs(artist_id);
CREATE INDEX IF NOT EXISTS idx_discs_discid ON discs(toc_discid) WHERE toc_discid IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_discs_type ON discs(type);
CREATE INDEX IF NOT EXISTS idx_discs_year ON discs(year);

-- ============================================================
-- Tracks
-- ============================================================

CREATE TABLE IF NOT EXISTS tracks (
    id                  INTEGER PRIMARY KEY,
    filepath            TEXT UNIQUE NOT NULL,
    filename            TEXT NOT NULL,
    disc_id             INTEGER REFERENCES discs(id) ON DELETE SET NULL,
    artist_id           INTEGER REFERENCES artists(id),
    album_artist_id     INTEGER REFERENCES artists(id),
    genre_id            INTEGER REFERENCES genres(id),
    title               TEXT,
    track_number        INTEGER,
    disc_number         INTEGER,
    year                INTEGER,
    duration_ms         INTEGER,
    bitrate             INTEGER,
    samplerate          INTEGER,
    channels            INTEGER,
    format              TEXT CHECK(format IN ('MP3','FLAC','OGG','OPUS','AAC','WAV','ALAC','APE','WV')),
    codec_profile       TEXT,
    md5_hash            TEXT,
    cover_hash          BLOB,
    play_count          INTEGER DEFAULT 0,
    skip_count          INTEGER DEFAULT 0,
    rating              REAL DEFAULT 0,
    comment             TEXT,
    missing             INTEGER DEFAULT 0,
    added_at            DATETIME DEFAULT CURRENT_TIMESTAMP,
    mtime               DATETIME,
    last_played         DATETIME
);

CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist_id);
CREATE INDEX IF NOT EXISTS idx_tracks_disc   ON tracks(disc_id);
CREATE INDEX IF NOT EXISTS idx_tracks_genre  ON tracks(genre_id);
CREATE INDEX IF NOT EXISTS idx_tracks_md5    ON tracks(md5_hash) WHERE md5_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_tracks_missing ON tracks(missing) WHERE missing = 1;
CREATE INDEX IF NOT EXISTS idx_tracks_last_played ON tracks(last_played DESC);

-- ============================================================
-- Playlists
-- ============================================================

CREATE TABLE IF NOT EXISTS playlists (
    id                  INTEGER PRIMARY KEY,
    name                TEXT NOT NULL,
    type                TEXT NOT NULL DEFAULT 'manual' CHECK(type IN ('manual','smart')),
    smart_rules_json    TEXT,
    limit_n             INTEGER,
    order_by            TEXT,
    live_update         INTEGER DEFAULT 1,
    created_at          DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS playlist_tracks (
    playlist_id         INTEGER REFERENCES playlists(id) ON DELETE CASCADE,
    track_id            INTEGER REFERENCES tracks(id) ON DELETE CASCADE,
    position            INTEGER NOT NULL,
    PRIMARY KEY(playlist_id, track_id, position)
);

CREATE INDEX IF NOT EXISTS idx_pltracks_playlist ON playlist_tracks(playlist_id, position);

-- ============================================================
-- Watched folders & remote sources
-- ============================================================

CREATE TABLE IF NOT EXISTS watched_folders (
    id          INTEGER PRIMARY KEY,
    path        TEXT UNIQUE NOT NULL,
    recursive   INTEGER DEFAULT 1,
    last_scan   DATETIME
);

CREATE TABLE IF NOT EXISTS remote_sources (
    id          INTEGER PRIMARY KEY,
    name        TEXT NOT NULL,
    base_url    TEXT NOT NULL,
    auth_token  TEXT,
    last_sync   DATETIME
);

-- ============================================================
-- Plugins
-- ============================================================

CREATE TABLE IF NOT EXISTS plugins (
    id          INTEGER PRIMARY KEY,
    name        TEXT NOT NULL,
    path        TEXT UNIQUE NOT NULL,
    type        TEXT CHECK(type IN ('native','winamp_vis','winamp_dsp')),
    enabled     INTEGER DEFAULT 1,
    config_json TEXT
);

-- ============================================================
-- Settings (key-value)
-- ============================================================

CREATE TABLE IF NOT EXISTS settings (
    key     TEXT PRIMARY KEY,
    value   TEXT
);

-- ============================================================
-- FTS5 indexes
-- ============================================================

CREATE VIRTUAL TABLE IF NOT EXISTS tracks_fts USING fts5(
    title,
    artist_name,
    album_title,
    genre_name,
    content='',
    tokenize='unicode61 remove_diacritics 2'
);

CREATE VIRTUAL TABLE IF NOT EXISTS discs_fts USING fts5(
    title,
    artist_name,
    content='',
    tokenize='unicode61 remove_diacritics 2'
);

-- Triggers to keep FTS in sync

CREATE TRIGGER IF NOT EXISTS tracks_ai AFTER INSERT ON tracks BEGIN
    INSERT INTO tracks_fts(rowid, title, artist_name, album_title, genre_name)
    VALUES (
        NEW.id,
        COALESCE(NEW.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = NEW.artist_id), ''),
        COALESCE((SELECT title FROM discs WHERE id = NEW.disc_id), ''),
        COALESCE((SELECT name FROM genres WHERE id = NEW.genre_id), '')
    );
END;

CREATE TRIGGER IF NOT EXISTS tracks_ad AFTER DELETE ON tracks BEGIN
    DELETE FROM tracks_fts WHERE rowid = OLD.id;
END;

CREATE TRIGGER IF NOT EXISTS tracks_au AFTER UPDATE ON tracks BEGIN
    DELETE FROM tracks_fts WHERE rowid = OLD.id;
    INSERT INTO tracks_fts(rowid, title, artist_name, album_title, genre_name)
    VALUES (
        NEW.id,
        COALESCE(NEW.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = NEW.artist_id), ''),
        COALESCE((SELECT title FROM discs WHERE id = NEW.disc_id), ''),
        COALESCE((SELECT name FROM genres WHERE id = NEW.genre_id), '')
    );
END;

CREATE TRIGGER IF NOT EXISTS discs_ai AFTER INSERT ON discs BEGIN
    INSERT INTO discs_fts(rowid, title, artist_name)
    VALUES (
        NEW.id,
        COALESCE(NEW.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = NEW.artist_id), '')
    );
END;

CREATE TRIGGER IF NOT EXISTS discs_ad AFTER DELETE ON discs BEGIN
    DELETE FROM discs_fts WHERE rowid = OLD.id;
END;

CREATE TRIGGER IF NOT EXISTS discs_au AFTER UPDATE ON discs BEGIN
    DELETE FROM discs_fts WHERE rowid = OLD.id;
    INSERT INTO discs_fts(rowid, title, artist_name)
    VALUES (
        NEW.id,
        COALESCE(NEW.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = NEW.artist_id), '')
    );
END;

INSERT INTO schema_version(version) VALUES (1);
