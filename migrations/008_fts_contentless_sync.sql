-- migrations/008_fts_contentless_sync.sql
--
-- FTS5 contentless tables (content='') do NOT allow DELETE statements:
-- SQLite 3.x raises "cannot DELETE from contentless fts5 table".
-- The four UPDATE/DELETE sync triggers created in migration 001 used
-- DELETE FROM <fts> WHERE rowid=OLD.id, which fails at runtime on every
-- UPDATE/DELETE on tracks or discs (including play_count bumps).
--
-- Fix: replace all DELETE-side triggers with the FTS5 'delete' special-insert
-- idiom — INSERT INTO fts(fts, rowid, col...) VALUES('delete', id, old_values...)
-- The caller must supply the OLD column values so SQLite can locate and remove
-- the tokens.  The two AFTER INSERT triggers (tracks_ai, discs_ai) are correct
-- and are left untouched.
--
-- This idiom works on all SQLite versions that support FTS5 (3.9+).
-- It does NOT require contentless_delete=1 (needs 3.43+).

DROP TRIGGER IF EXISTS tracks_ad;
CREATE TRIGGER tracks_ad AFTER DELETE ON tracks BEGIN
    INSERT INTO tracks_fts(tracks_fts, rowid, title, artist_name, album_title, genre_name)
    VALUES (
        'delete',
        OLD.id,
        COALESCE(OLD.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = OLD.artist_id), ''),
        COALESCE((SELECT title FROM discs WHERE id = OLD.disc_id), ''),
        COALESCE((SELECT name FROM genres WHERE id = OLD.genre_id), '')
    );
END;

DROP TRIGGER IF EXISTS tracks_au;
CREATE TRIGGER tracks_au AFTER UPDATE ON tracks BEGIN
    INSERT INTO tracks_fts(tracks_fts, rowid, title, artist_name, album_title, genre_name)
    VALUES (
        'delete',
        OLD.id,
        COALESCE(OLD.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = OLD.artist_id), ''),
        COALESCE((SELECT title FROM discs WHERE id = OLD.disc_id), ''),
        COALESCE((SELECT name FROM genres WHERE id = OLD.genre_id), '')
    );
    INSERT INTO tracks_fts(rowid, title, artist_name, album_title, genre_name)
    VALUES (
        NEW.id,
        COALESCE(NEW.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = NEW.artist_id), ''),
        COALESCE((SELECT title FROM discs WHERE id = NEW.disc_id), ''),
        COALESCE((SELECT name FROM genres WHERE id = NEW.genre_id), '')
    );
END;

DROP TRIGGER IF EXISTS discs_ad;
CREATE TRIGGER discs_ad AFTER DELETE ON discs BEGIN
    INSERT INTO discs_fts(discs_fts, rowid, title, artist_name)
    VALUES (
        'delete',
        OLD.id,
        COALESCE(OLD.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = OLD.artist_id), '')
    );
END;

DROP TRIGGER IF EXISTS discs_au;
CREATE TRIGGER discs_au AFTER UPDATE ON discs BEGIN
    INSERT INTO discs_fts(discs_fts, rowid, title, artist_name)
    VALUES (
        'delete',
        OLD.id,
        COALESCE(OLD.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = OLD.artist_id), '')
    );
    INSERT INTO discs_fts(rowid, title, artist_name)
    VALUES (
        NEW.id,
        COALESCE(NEW.title, ''),
        COALESCE((SELECT name FROM artists WHERE id = NEW.artist_id), '')
    );
END;

INSERT INTO schema_version(version) VALUES (8);
