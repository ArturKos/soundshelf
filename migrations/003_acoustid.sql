-- migrations/003_acoustid.sql
-- AcoustID + MusicBrainz Recording IDs for fingerprinting

ALTER TABLE tracks ADD COLUMN acoustid TEXT;
ALTER TABLE tracks ADD COLUMN mb_recording_id TEXT;

CREATE INDEX IF NOT EXISTS idx_tracks_acoustid ON tracks(acoustid) WHERE acoustid IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_tracks_mbrec ON tracks(mb_recording_id) WHERE mb_recording_id IS NOT NULL;

INSERT INTO schema_version(version) VALUES (3);
