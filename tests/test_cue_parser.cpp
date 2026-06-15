#include <QtTest>
#include <QMap>
#include "soundshelf/io/CueParser.hpp"

using namespace soundshelf;

class TestCueParser : public QObject {
    Q_OBJECT

private slots:

    void framesToMsRoundsCorrectly() {
        // 75 fps CD-DA: 75 frames = 1000 ms exactly.
        QCOMPARE(CueParser::framesToMs(0),     0);
        QCOMPARE(CueParser::framesToMs(75),    1000);
        QCOMPARE(CueParser::framesToMs(150),   2000);
        // 7500 frames = 100 s = 100000 ms.
        QCOMPARE(CueParser::framesToMs(7500),  100000);
        // negatives clamped to zero.
        QCOMPARE(CueParser::framesToMs(-5),    0);
    }

    void parsesMinimalSheet() {
        const QString cue = R"(FILE "album.flac" WAVE
TITLE "My Album"
PERFORMER "Some Band"
REM DATE 2024
TRACK 01 AUDIO
  TITLE "First"
  PERFORMER "Some Band"
  INDEX 01 00:00:00
TRACK 02 AUDIO
  TITLE "Second"
  INDEX 00 03:14:00
  INDEX 01 03:15:00
TRACK 03 AUDIO
  TITLE "Third"
  INDEX 01 06:30:00
)";
        CueParser parser;
        auto r = parser.parseString(cue, QStringLiteral("test"));
        QVERIFY(r.isOk());
        const auto& sheet = r.value();
        QCOMPARE(sheet.albumTitle,     QStringLiteral("My Album"));
        QCOMPARE(sheet.albumPerformer, QStringLiteral("Some Band"));
        QCOMPARE(sheet.year,           2024);
        QCOMPARE(sheet.file,           QStringLiteral("album.flac"));
        QCOMPARE(sheet.fileType,       QStringLiteral("WAVE"));
        QCOMPARE(sheet.tracks.size(),  3);

        QCOMPARE(sheet.tracks[0].trackNumber,    1);
        QCOMPARE(sheet.tracks[0].title,          QStringLiteral("First"));
        QCOMPARE(sheet.tracks[0].indexOneFrames, 0L);

        QCOMPARE(sheet.tracks[1].title,          QStringLiteral("Second"));
        QCOMPARE(sheet.tracks[1].indexZeroFrames, 3L * 60 * 75 + 14 * 75);
        QCOMPARE(sheet.tracks[1].indexOneFrames,  3L * 60 * 75 + 15 * 75);

        // Single-file: CueTrack::file must be empty, isMultiFile must be false.
        QVERIFY(sheet.tracks[0].file.isEmpty());
        QVERIFY(sheet.tracks[1].file.isEmpty());
        QVERIFY(sheet.tracks[2].file.isEmpty());
        QVERIFY(!sheet.isMultiFile());
    }

    void readsReplayGainRems() {
        const QString cue = R"(FILE "x.flac" WAVE
REM REPLAYGAIN_ALBUM_GAIN -7.50
REM REPLAYGAIN_ALBUM_PEAK 0.99
TRACK 01 AUDIO
  REM REPLAYGAIN_TRACK_GAIN -8.20
  REM REPLAYGAIN_TRACK_PEAK 0.95
  INDEX 01 00:00:00
)";
        CueParser parser;
        auto r = parser.parseString(cue);
        QVERIFY(r.isOk());
        const auto& s = r.value();
        QVERIFY(s.rgAlbumGain.has_value());
        QCOMPARE(*s.rgAlbumGain, -7.50);
        QCOMPARE(*s.rgAlbumPeak, 0.99);
        QVERIFY(s.tracks[0].rgTrackGain.has_value());
        QCOMPARE(*s.tracks[0].rgTrackGain, -8.20);
        QCOMPARE(*s.tracks[0].rgTrackPeak, 0.95);
    }

    void rejectsCueWithoutTracks() {
        const QString cue = QStringLiteral("FILE \"x.flac\" WAVE\nTITLE \"Empty\"\n");
        CueParser parser;
        auto r = parser.parseString(cue);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidFormat);
    }

    void tocFromSheetComputesDurations() {
        const QString cue = R"(FILE "x.flac" WAVE
TRACK 01 AUDIO
  INDEX 01 00:00:00
TRACK 02 AUDIO
  INDEX 01 02:30:00
TRACK 03 AUDIO
  INDEX 01 05:00:00
)";
        auto r = CueParser().parseString(cue);
        QVERIFY(r.isOk());
        // Last track length comes from totalAudioMs = 7 minutes.
        const Toc toc = CueParser::tocFromSheet(r.value(), 7 * 60 * 1000);
        QCOMPARE(toc.entries.size(), 3);
        QCOMPARE(toc.entries[0].durationMs, 150 * 1000);   // 2:30
        QCOMPARE(toc.entries[1].durationMs, 150 * 1000);   // 2:30
        QCOMPARE(toc.entries[2].durationMs, 120 * 1000);   // 7:00 - 5:00 = 2:00
        QCOMPARE(toc.totalDurationMs,       420 * 1000);
    }

    void handlesQuotedTitlesAndComments() {
        const QString cue = R"(REM GENRE "Indie Rock"
FILE "weird name with spaces.flac" WAVE
TITLE "An ""album"" name"
TRACK 01 AUDIO
  TITLE "Track 1"
  INDEX 01 00:00:00
)";
        auto r = CueParser().parseString(cue);
        QVERIFY(r.isOk());
        const auto& s = r.value();
        QCOMPARE(s.file,        QStringLiteral("weird name with spaces.flac"));
        QCOMPARE(s.albumGenre,  QStringLiteral("Indie Rock"));
    }

    // ---- Multi-file tests ------------------------------------------------

    void multiFileSheetAssignsTrackFiles() {
        // 3 FILE lines, each with exactly one TRACK — typical EAC per-track rip.
        const QString cue = R"(FILE "track01.flac" WAVE
TRACK 01 AUDIO
  TITLE "Song One"
  INDEX 01 00:00:00
FILE "track02.flac" WAVE
TRACK 02 AUDIO
  TITLE "Song Two"
  INDEX 01 00:00:00
FILE "track03.flac" WAVE
TRACK 03 AUDIO
  TITLE "Song Three"
  INDEX 01 00:00:00
)";
        CueParser parser;
        auto r = parser.parseString(cue, QStringLiteral("multi-test"));
        QVERIFY(r.isOk());
        const auto& sheet = r.value();

        QCOMPARE(sheet.tracks.size(), 3);
        QVERIFY(sheet.isMultiFile());

        // sheet.file must be the first FILE encountered.
        QCOMPARE(sheet.file,     QStringLiteral("track01.flac"));
        QCOMPARE(sheet.fileType, QStringLiteral("WAVE"));

        // Each track must carry its own container name.
        QCOMPARE(sheet.tracks[0].file, QStringLiteral("track01.flac"));
        QCOMPARE(sheet.tracks[1].file, QStringLiteral("track02.flac"));
        QCOMPARE(sheet.tracks[2].file, QStringLiteral("track03.flac"));

        // Track titles must be preserved.
        QCOMPARE(sheet.tracks[0].title, QStringLiteral("Song One"));
        QCOMPARE(sheet.tracks[1].title, QStringLiteral("Song Two"));
        QCOMPARE(sheet.tracks[2].title, QStringLiteral("Song Three"));
    }

    void tocFromSheetMultiFileEachOwnFile() {
        // Each of 3 tracks resides in its own file; INDEX 01 is 00:00:00 per file.
        const QString cue = R"(FILE "track01.flac" WAVE
TRACK 01 AUDIO
  TITLE "Song One"
  INDEX 01 00:00:00
FILE "track02.flac" WAVE
TRACK 02 AUDIO
  TITLE "Song Two"
  INDEX 01 00:00:00
FILE "track03.flac" WAVE
TRACK 03 AUDIO
  TITLE "Song Three"
  INDEX 01 00:00:00
)";
        auto r = CueParser().parseString(cue);
        QVERIFY(r.isOk());
        QVERIFY(r.value().isMultiFile());

        // Durations come entirely from the per-file length map.
        QMap<QString, int> durations;
        durations[QStringLiteral("track01.flac")] = 3 * 60 * 1000;   // 3 min
        durations[QStringLiteral("track02.flac")] = 4 * 60 * 1000;   // 4 min
        durations[QStringLiteral("track03.flac")] = 5 * 60 * 1000;   // 5 min

        const Toc toc = CueParser::tocFromSheet(r.value(), durations);
        QCOMPARE(toc.entries.size(), 3);
        QCOMPARE(toc.entries[0].durationMs, 180 * 1000);  // 3:00
        QCOMPARE(toc.entries[1].durationMs, 240 * 1000);  // 4:00
        QCOMPARE(toc.entries[2].durationMs, 300 * 1000);  // 5:00
        QCOMPARE(toc.totalDurationMs,       720 * 1000);
    }

    void tocFromSheetMultiFileMixed() {
        // disc1.flac holds tracks 1–2; disc2.flac holds track 3.
        // Track 1 ends at track 2's INDEX 01; track 2 runs to disc1 end;
        // track 3 runs to disc2 end.
        const QString cue = R"(FILE "disc1.flac" WAVE
TRACK 01 AUDIO
  TITLE "Track One"
  INDEX 01 00:00:00
TRACK 02 AUDIO
  TITLE "Track Two"
  INDEX 01 03:30:00
FILE "disc2.flac" WAVE
TRACK 03 AUDIO
  TITLE "Track Three"
  INDEX 01 00:00:00
)";
        auto r = CueParser().parseString(cue);
        QVERIFY(r.isOk());
        QVERIFY(r.value().isMultiFile());

        // disc1 = 7 min (420 s), disc2 = 5 min (300 s).
        QMap<QString, int> durations;
        durations[QStringLiteral("disc1.flac")] = 7 * 60 * 1000;
        durations[QStringLiteral("disc2.flac")] = 5 * 60 * 1000;

        const Toc toc = CueParser::tocFromSheet(r.value(), durations);
        QCOMPARE(toc.entries.size(), 3);

        // Track 1: 0 → 3:30 = 210 s
        QCOMPARE(toc.entries[0].durationMs, 210 * 1000);
        // Track 2: 3:30 → 7:00 = 210 s
        QCOMPARE(toc.entries[1].durationMs, 210 * 1000);
        // Track 3: 0 → 5:00 = 300 s
        QCOMPARE(toc.entries[2].durationMs, 300 * 1000);
        QCOMPARE(toc.totalDurationMs, 720 * 1000);
    }

    void singleFileSheetIsNotMultiFile() {
        // Regression: a sheet with one FILE must report isMultiFile() == false.
        const QString cue = R"(FILE "album.flac" WAVE
TRACK 01 AUDIO
  INDEX 01 00:00:00
TRACK 02 AUDIO
  INDEX 01 04:00:00
)";
        auto r = CueParser().parseString(cue);
        QVERIFY(r.isOk());
        QVERIFY(!r.value().isMultiFile());

        // CueTrack::file must be empty for single-file sheets.
        QVERIFY(r.value().tracks[0].file.isEmpty());
        QVERIFY(r.value().tracks[1].file.isEmpty());

        // Existing single-file tocFromSheet must still work.
        const Toc toc = CueParser::tocFromSheet(r.value(), 10 * 60 * 1000);
        QCOMPARE(toc.entries.size(), 2);
        QCOMPARE(toc.entries[0].durationMs, 4 * 60 * 1000);   // 4:00
        QCOMPARE(toc.entries[1].durationMs, 6 * 60 * 1000);   // 10:00 - 4:00 = 6:00
        QCOMPARE(toc.totalDurationMs, 10 * 60 * 1000);
    }
};

QTEST_APPLESS_MAIN(TestCueParser)
#include "test_cue_parser.moc"
