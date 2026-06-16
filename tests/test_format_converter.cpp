#include <QTest>
#include "soundshelf/io/FormatConverter.hpp"

using namespace soundshelf;
using F = FormatConverter::Format;

class TestFormatConverter : public QObject {
    Q_OBJECT

private slots:
    // ---------- extensionForFormat ----------

    void extension_Mp3V0_returnsMp3()   { QCOMPARE(FormatConverter::extensionForFormat(F::Mp3V0),    QStringLiteral("mp3")); }
    void extension_Mp3_320_returnsMp3() { QCOMPARE(FormatConverter::extensionForFormat(F::Mp3_320),  QStringLiteral("mp3")); }
    void extension_OggVorbis_returnsOgg() { QCOMPARE(FormatConverter::extensionForFormat(F::OggVorbis), QStringLiteral("ogg")); }
    void extension_Opus128_returnsOpus() { QCOMPARE(FormatConverter::extensionForFormat(F::Opus_128), QStringLiteral("opus")); }
    void extension_Aac256_returnsM4a() { QCOMPARE(FormatConverter::extensionForFormat(F::Aac_256),  QStringLiteral("m4a")); }
    void extension_Flac_returnsFlac()  { QCOMPARE(FormatConverter::extensionForFormat(F::Flac),     QStringLiteral("flac")); }
    void extension_WavPcm16_returnsWav() { QCOMPARE(FormatConverter::extensionForFormat(F::WavPcm16), QStringLiteral("wav")); }

    // ---------- buildArguments — common flags ----------

    void build_alwaysContainsHideBanner()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3V0;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("-hide_banner")));
        QVERIFY(args.contains(QStringLiteral("-nostdin")));
    }

    void build_alwaysStripsVideo()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3V0;
        QVERIFY(FormatConverter::buildArguments(j).contains(QStringLiteral("-vn")));
    }

    void build_alwaysPreservesMetadata()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3V0;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("-map_metadata")));
        QVERIFY(args.contains(QStringLiteral("0")));
    }

    void build_outputIsLastArg()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3V0;
        const auto args = FormatConverter::buildArguments(j);
        QCOMPARE(args.last(), QStringLiteral("/out.mp3"));
    }

    void build_inputFollowsMinusI()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3V0;
        const auto args = FormatConverter::buildArguments(j);
        const int idx = args.indexOf(QStringLiteral("-i"));
        QVERIFY(idx >= 0);
        QCOMPARE(args.at(idx + 1), QStringLiteral("/in.flac"));
    }

    // ---------- buildArguments — overwrite flag ----------

    void build_overwriteFalse_hasMinusN_notMinusY()
    {
        FormatConverter::Job j;
        j.input    = QStringLiteral("/in.flac");
        j.output   = QStringLiteral("/out.mp3");
        j.format   = F::Mp3V0;
        j.overwrite = false;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("-n")));
        QVERIFY(!args.contains(QStringLiteral("-y")));
    }

    void build_overwriteTrue_hasMinusY_notMinusN()
    {
        FormatConverter::Job j;
        j.input    = QStringLiteral("/in.flac");
        j.output   = QStringLiteral("/out.mp3");
        j.format   = F::Mp3V0;
        j.overwrite = true;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("-y")));
        QVERIFY(!args.contains(QStringLiteral("-n")));
    }

    // ---------- buildArguments — per-format codec flags ----------

    void build_Mp3V0_hasLameAndQ0()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3V0;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("libmp3lame")));
        QVERIFY(args.contains(QStringLiteral("-q:a")));
        const int qi = args.indexOf(QStringLiteral("-q:a"));
        QCOMPARE(args.at(qi + 1), QStringLiteral("0"));
    }

    void build_Mp3_320_hasLameAnd320k()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.mp3");
        j.format = F::Mp3_320;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("libmp3lame")));
        QVERIFY(args.contains(QStringLiteral("-b:a")));
        const int bi = args.indexOf(QStringLiteral("-b:a"));
        QCOMPARE(args.at(bi + 1), QStringLiteral("320k"));
    }

    void build_OggVorbis_hasVorbisAndQ6()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.ogg");
        j.format = F::OggVorbis;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("libvorbis")));
        QVERIFY(args.contains(QStringLiteral("-q:a")));
        const int qi = args.indexOf(QStringLiteral("-q:a"));
        QCOMPARE(args.at(qi + 1), QStringLiteral("6"));
    }

    void build_Opus128_hasLibopusAnd128k()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.opus");
        j.format = F::Opus_128;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("libopus")));
        QVERIFY(args.contains(QStringLiteral("-b:a")));
        const int bi = args.indexOf(QStringLiteral("-b:a"));
        QCOMPARE(args.at(bi + 1), QStringLiteral("128k"));
    }

    void build_Aac256_hasAacAnd256k()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.m4a");
        j.format = F::Aac_256;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("aac")));
        QVERIFY(args.contains(QStringLiteral("-b:a")));
        const int bi = args.indexOf(QStringLiteral("-b:a"));
        QCOMPARE(args.at(bi + 1), QStringLiteral("256k"));
    }

    void build_Flac_hasFlacAndCompressionLevel8()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.wav");
        j.output = QStringLiteral("/out.flac");
        j.format = F::Flac;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("flac")));
        QVERIFY(args.contains(QStringLiteral("-compression_level")));
        const int ci = args.indexOf(QStringLiteral("-compression_level"));
        QCOMPARE(args.at(ci + 1), QStringLiteral("8"));
    }

    void build_WavPcm16_hasPcmS16le()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.wav");
        j.format = F::WavPcm16;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("pcm_s16le")));
    }

    // ---------- buildArguments — optional overrides ----------

    void build_samplerateOverride_addsMinusAr()
    {
        FormatConverter::Job j;
        j.input             = QStringLiteral("/in.flac");
        j.output            = QStringLiteral("/out.flac");
        j.format            = F::Flac;
        j.samplerateOverride = 44100;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("-ar")));
        const int ai = args.indexOf(QStringLiteral("-ar"));
        QCOMPARE(args.at(ai + 1), QStringLiteral("44100"));
    }

    void build_channelsOverride_addsMinusAc()
    {
        FormatConverter::Job j;
        j.input           = QStringLiteral("/in.flac");
        j.output          = QStringLiteral("/out.flac");
        j.format          = F::Flac;
        j.channelsOverride = 2;
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(args.contains(QStringLiteral("-ac")));
        const int ai = args.indexOf(QStringLiteral("-ac"));
        QCOMPARE(args.at(ai + 1), QStringLiteral("2"));
    }

    void build_noOverrides_noArAc()
    {
        FormatConverter::Job j;
        j.input  = QStringLiteral("/in.flac");
        j.output = QStringLiteral("/out.flac");
        j.format = F::Flac;
        // samplerateOverride and channelsOverride default to 0
        const auto args = FormatConverter::buildArguments(j);
        QVERIFY(!args.contains(QStringLiteral("-ar")));
        QVERIFY(!args.contains(QStringLiteral("-ac")));
    }
};

QTEST_MAIN(TestFormatConverter)
#include "test_format_converter.moc"
