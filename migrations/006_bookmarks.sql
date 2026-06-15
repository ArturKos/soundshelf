-- migrations/006_bookmarks.sql
-- Audiobook/podcast resume positions and named bookmarks (feature #12)

CREATE TABLE IF NOT EXISTS bookmarks (
    id          INTEGER PRIMARY KEY,
    track_id    INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
    position_ms INTEGER NOT NULL,
    label       TEXT,
    is_resume   INTEGER NOT NULL DEFAULT 0,
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_bookmarks_track ON bookmarks(track_id);

-- At most one resume marker per track
CREATE UNIQUE INDEX IF NOT EXISTS idx_bookmarks_resume ON bookmarks(track_id) WHERE is_resume = 1;

INSERT INTO schema_version(version) VALUES (6);
