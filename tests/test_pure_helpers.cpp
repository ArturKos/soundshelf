#include <QtTest>
#include "soundshelf/io/DiscRipper.hpp"
#include "soundshelf/io/FormatConverter.hpp"

using namespace soundshelf;

class TestPureHelpers : public QObject {
    Q_OBJECT

private slots:

    // ---- DiscRipper::applyPattern --------------------------------------

    void ripperPatternBasicSubstitution() {
        const QString out = DiscRipper::applyPattern(
            QStringLiteral("%track%-%title%.%ext%"),
            5, QStringLiteral("Hey Joe"), QStringLiteral("Hendrix"),
            QStringLiteral("Are You Experienced"), 1967, QStringLiteral("flac"));
        QCOMPARE(out, QStringLiteral("05-Hey Joe.flac"));
    }

    void ripperPatternZeroPadsTrackNumber() {
        const QString out = DiscRipper::applyPattern(
            QStringLiteral("%track%"), 1, {}, {}, {}, 0, QStringLiteral("flac"));
        QCOMPARE(out, QStringLiteral("01"));
    }

    void ripperPatternSanitisesFilenameChars() {
        // Path-hostile chars in title should be stripped.
        const QString out = DiscRipper::applyPattern(
            QStringLiteral("%title%.%ext%"),
            1, QStringLiteral("AC/DC: Back?"),
            {}, {}, 0, QStringLiteral("mp3"));
        QVERIFY(!out.contains(QLatin1Char('/')));
        QVERIFY(!out.contains(QLatin1Char(':')));
        QVERIFY(!out.contains(QLatin1Char('?')));
        QVERIFY(out.endsWith(QStringLiteral(".mp3")));
    }

    void ripperPatternEmptyTitleFallsBackToUntitled() {
        const QString out = DiscRipper::applyPattern(
            QStringLiteral("%title%"), 0, QString(), {}, {}, 0,
            QStringLiteral("flac"));
        QCOMPARE(out, QStringLiteral("untitled"));
    }

    void ripperPatternHandlesAllPlaceholders() {
        const QString out = DiscRipper::applyPattern(
            QStringLiteral("%artist%/%album%/%track%-%title%-(%year%).%ext%"),
            12, QStringLiteral("T"), QStringLiteral("A"),
            QStringLiteral("Alb"), 2003, QStringLiteral("flac"));
        QCOMPARE(out, QStringLiteral("A/Alb/12-T-(2003).flac"));
    }

    // ---- FormatConverter::buildArguments -------------------------------

    void converterArgsForFlac() {
        FormatConverter::Job job;
        job.input  = QStringLiteral("/in.wav");
        job.output = QStringLiteral("/out.flac");
        job.format = FormatConverter::Format::Flac;
        const QStringList args = FormatConverter::buildArguments(job);

        // Sanity: input + output positions, codec, defaults.
        QVERIFY(args.contains(QStringLiteral("-i")));
        QCOMPARE(args.last(), QStringLiteral("/out.flac"));
        QCOMPARE(args[args.indexOf(QStringLiteral("-i")) + 1],
                 QStringLiteral("/in.wav"));
        QVERIFY(args.contains(QStringLiteral("flac")));
        QVERIFY(args.contains(QStringLiteral("-c:a")));
        QVERIFY(args.contains(QStringLiteral("-vn")));   // strip video
        QVERIFY(args.contains(QStringLiteral("-map_metadata")));
        // Defaults to no-overwrite.
        QVERIFY(args.contains(QStringLiteral("-n")));
    }

    void converterArgsRespectOverwrite() {
        FormatConverter::Job job;
        job.input = QStringLiteral("a"); job.output = QStringLiteral("b");
        job.format = FormatConverter::Format::Mp3V0;
        job.overwrite = true;
        const QStringList args = FormatConverter::buildArguments(job);
        QVERIFY(args.contains(QStringLiteral("-y")));
        QVERIFY(!args.contains(QStringLiteral("-n")));
    }

    void converterArgsEmitMp3V0Quality() {
        FormatConverter::Job job;
        job.input = QStringLiteral("a"); job.output = QStringLiteral("b");
        job.format = FormatConverter::Format::Mp3V0;
        const QStringList args = FormatConverter::buildArguments(job);
        QVERIFY(args.contains(QStringLiteral("libmp3lame")));
        const int qIdx = args.indexOf(QStringLiteral("-q:a"));
        QVERIFY(qIdx >= 0);
        QCOMPARE(args[qIdx + 1], QStringLiteral("0"));
    }

    void converterArgsAppendsOverrides() {
        FormatConverter::Job job;
        job.input = QStringLiteral("a"); job.output = QStringLiteral("b.flac");
        job.format = FormatConverter::Format::Flac;
        job.samplerateOverride = 48000;
        job.channelsOverride = 2;
        const QStringList args = FormatConverter::buildArguments(job);
        const int sr = args.indexOf(QStringLiteral("-ar"));
        const int ch = args.indexOf(QStringLiteral("-ac"));
        QVERIFY(sr >= 0);  QCOMPARE(args[sr + 1], QStringLiteral("48000"));
        QVERIFY(ch >= 0);  QCOMPARE(args[ch + 1], QStringLiteral("2"));
    }
};

QTEST_APPLESS_MAIN(TestPureHelpers)
#include "test_pure_helpers.moc"
