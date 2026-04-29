#include <QtTest>
#include "soundshelf/core/Track.hpp"

using namespace soundshelf;

class TestTrackFormat : public QObject {
    Q_OBJECT

private slots:

    void formatToString() {
        QCOMPARE(audioFormatToString(AudioFormat::MP3),  QStringLiteral("MP3"));
        QCOMPARE(audioFormatToString(AudioFormat::FLAC), QStringLiteral("FLAC"));
        QCOMPARE(audioFormatToString(AudioFormat::Unknown), QStringLiteral("UNKNOWN"));
    }

    void formatFromString() {
        QCOMPARE(audioFormatFromString("mp3"),  AudioFormat::MP3);
        QCOMPARE(audioFormatFromString("MP3"),  AudioFormat::MP3);
        QCOMPARE(audioFormatFromString("Flac"), AudioFormat::FLAC);
        QCOMPARE(audioFormatFromString("vorbis"), AudioFormat::OGG);
        QCOMPARE(audioFormatFromString("m4a"),  AudioFormat::AAC);
        QCOMPARE(audioFormatFromString("xyz"),  AudioFormat::Unknown);
    }

    void formatFromFilename() {
        QCOMPARE(audioFormatFromFilename("/path/to/song.mp3"),  AudioFormat::MP3);
        QCOMPARE(audioFormatFromFilename("/path/to/song.FLAC"), AudioFormat::FLAC);
        QCOMPARE(audioFormatFromFilename("/path/song.opus"),    AudioFormat::OPUS);
        QCOMPARE(audioFormatFromFilename("/path/song.m4a"),     AudioFormat::AAC);
        QCOMPARE(audioFormatFromFilename("/path/song.txt"),     AudioFormat::Unknown);
    }

    void discTypeRoundtrip() {
        for (auto t : { DiscType::Physical, DiscType::Folder,
                        DiscType::Image, DiscType::Remote }) {
            const auto s = discTypeToString(t);
            QCOMPARE(discTypeFromString(s), t);
        }
    }

    void replayGainEffective() {
        Track t;
        QCOMPARE(t.effectiveReplayGainDb(false), 0.0);

        t.rgTrackGain = -6.5;
        QCOMPARE(t.effectiveReplayGainDb(false), -6.5);

        t.rgAlbumGain = -7.0;
        QCOMPARE(t.effectiveReplayGainDb(true), -7.0);
        QCOMPARE(t.effectiveReplayGainDb(false), -6.5);
    }
};

QTEST_MAIN(TestTrackFormat)
#include "test_track_format.moc"
