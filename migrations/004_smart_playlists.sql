-- migrations/004_smart_playlists.sql
-- Already covered in 001 (smart_rules_json column on playlists)
-- This migration adds support for hierarchical rules and tags

CREATE TABLE IF NOT EXISTS tags (
    id          INTEGER PRIMARY KEY,
    name        TEXT UNIQUE NOT NULL,
    color       TEXT
);

CREATE TABLE IF NOT EXISTS track_tags (
    track_id    INTEGER REFERENCES tracks(id) ON DELETE CASCADE,
    tag_id      INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY(track_id, tag_id)
);

INSERT INTO schema_version(version) VALUES (4);
