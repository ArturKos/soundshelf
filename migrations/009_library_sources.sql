-- migrations/009_library_sources.sql
-- Library sources: each imported folder appears as a named source row.
-- Tracks carry an optional source_id FK so they can be filtered by source.

CREATE TABLE library_sources(
    id       INTEGER PRIMARY KEY,
    path     TEXT UNIQUE NOT NULL,
    label    TEXT NOT NULL,
    added_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

ALTER TABLE tracks ADD COLUMN source_id INTEGER REFERENCES library_sources(id) ON DELETE SET NULL;

CREATE INDEX idx_tracks_source ON tracks(source_id);

INSERT INTO schema_version(version) VALUES (9);
