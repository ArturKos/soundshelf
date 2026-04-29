-- migrations/005_play_history.sql
-- Play history, scrobble queue, lyrics

CREATE TABLE IF NOT EXISTS play_history (
    id          INTEGER PRIMARY KEY,
    track_id    INTEGER REFERENCES tracks(id) ON DELETE CASCADE,
    played_at   DATETIME DEFAULT CURRENT_TIMESTAMP,
    played_ms   INTEGER NOT NULL,       -- jak długo faktycznie grany
    completed   INTEGER DEFAULT 0,      -- czy odsłuchany do końca
    source      TEXT                    -- 'gui', 'cli', 'remote', 'mpris'
);

CREATE INDEX IF NOT EXISTS idx_history_track ON play_history(track_id);
CREATE INDEX IF NOT EXISTS idx_history_date  ON play_history(played_at DESC);

CREATE TABLE IF NOT EXISTS scrobble_queue (
    id          INTEGER PRIMARY KEY,
    track_id    INTEGER REFERENCES tracks(id) ON DELETE CASCADE,
    service     TEXT NOT NULL CHECK(service IN ('lastfm','listenbrainz')),
    queued_at   DATETIME DEFAULT CURRENT_TIMESTAMP,
    sent        INTEGER DEFAULT 0,
    response    TEXT,
    retry_count INTEGER DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_scrobble_pending ON scrobble_queue(sent, queued_at) WHERE sent = 0;

CREATE TABLE IF NOT EXISTS lyrics (
    track_id    INTEGER PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
    text        TEXT,
    synced_lrc  TEXT,                   -- LRC format z timestampami
    source      TEXT,                   -- 'lrclib', 'manual', 'embedded'
    fetched_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO schema_version(version) VALUES (5);
