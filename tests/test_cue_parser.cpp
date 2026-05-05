#include <QtTest>
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
};

QTEST_APPLESS_MAIN(TestCueParser)
#include "test_cue_parser.moc"
