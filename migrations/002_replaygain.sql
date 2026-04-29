-- migrations/002_replaygain.sql
-- ReplayGain / EBU R128 columns

ALTER TABLE tracks ADD COLUMN rg_track_gain REAL;
ALTER TABLE tracks ADD COLUMN rg_track_peak REAL;
ALTER TABLE tracks ADD COLUMN rg_album_gain REAL;
ALTER TABLE tracks ADD COLUMN rg_album_peak REAL;

CREATE INDEX IF NOT EXISTS idx_tracks_rg_track ON tracks(rg_track_gain) WHERE rg_track_gain IS NOT NULL;

INSERT INTO schema_version(version) VALUES (2);
