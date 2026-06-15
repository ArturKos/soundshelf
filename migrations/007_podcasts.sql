-- migrations/007_podcasts.sql
-- Podcast feeds and episodes persistence (feature #12, DB-schema half)

CREATE TABLE IF NOT EXISTS podcast_feeds (
    id             INTEGER PRIMARY KEY,
    url            TEXT NOT NULL UNIQUE,
    title          TEXT,
    author         TEXT,
    description    TEXT,
    image_url      TEXT,
    link           TEXT,
    language       TEXT,
    last_refreshed DATETIME,
    added_at       DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS podcast_episodes (
    id               INTEGER PRIMARY KEY,
    feed_id          INTEGER NOT NULL REFERENCES podcast_feeds(id) ON DELETE CASCADE,
    guid             TEXT NOT NULL,
    title            TEXT,
    description      TEXT,
    enclosure_url    TEXT,
    enclosure_type   TEXT,
    enclosure_length INTEGER DEFAULT 0,
    pub_date         DATETIME,
    duration_ms      INTEGER DEFAULT 0,
    episode_number   INTEGER DEFAULT 0,
    is_played        INTEGER NOT NULL DEFAULT 0,
    local_path       TEXT
);

CREATE INDEX IF NOT EXISTS idx_podcast_episodes_feed ON podcast_episodes(feed_id);

CREATE UNIQUE INDEX IF NOT EXISTS idx_podcast_episodes_guid ON podcast_episodes(feed_id, guid);

INSERT INTO schema_version(version) VALUES (7);
