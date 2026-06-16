#include <QtTest>
#include "soundshelf/io/LrcParser.hpp"

using namespace soundshelf;

class TestLrcParser : public QObject {
    Q_OBJECT

private slots:

    // Criterion 1: a line with multiple leading timestamp tags produces one LrcLine per timestamp
    void multipleTimestampsPerLine() {
        const QString lrc = QStringLiteral("[00:12.00][00:45.30]Hello");
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.lines.size(), 2);
        // After stable_sort by timeMs:
        QCOMPARE(doc.lines[0].timeMs, 12000);
        QCOMPARE(doc.lines[0].text,   QStringLiteral("Hello"));
        QCOMPARE(doc.lines[1].timeMs, 45300);
        QCOMPARE(doc.lines[1].text,   QStringLiteral("Hello"));
    }

    // Criterion 2: fraction-digit handling — 1/2/3 digits all yield the right ms value
    void fractionHandling() {
        // 2-digit = centiseconds: .50 → 50*10 = 500ms  → total 12500ms
        {
            const auto doc = LrcParser::parse(QStringLiteral("[00:12.50]A"));
            QVERIFY(!doc.lines.isEmpty());
            QCOMPARE(doc.lines[0].timeMs, 12500);
        }
        // 3-digit = milliseconds: .500 → 500ms → total 12500ms
        {
            const auto doc = LrcParser::parse(QStringLiteral("[00:12.500]B"));
            QVERIFY(!doc.lines.isEmpty());
            QCOMPARE(doc.lines[0].timeMs, 12500);
        }
        // 1-digit = tenths: .5 → 5*100 = 500ms → total 12500ms
        {
            const auto doc = LrcParser::parse(QStringLiteral("[00:12.5]C"));
            QVERIFY(!doc.lines.isEmpty());
            QCOMPARE(doc.lines[0].timeMs, 12500);
        }
        // [01:02.00] → (62)*1000 + 0 = 62000ms
        {
            const auto doc = LrcParser::parse(QStringLiteral("[01:02.00]D"));
            QVERIFY(!doc.lines.isEmpty());
            QCOMPARE(doc.lines[0].timeMs, 62000);
        }
        // No fraction: [00:05]line → 5000ms
        {
            const auto doc = LrcParser::parse(QStringLiteral("[00:05]E"));
            QVERIFY(!doc.lines.isEmpty());
            QCOMPARE(doc.lines[0].timeMs, 5000);
        }
    }

    // Criterion 3: metadata/ID tags populate metadata map and are NOT emitted as timed lines
    void metadataTags() {
        const QString lrc =
            "[ar:Test Artist]\n"
            "[ti:My Song]\n"
            "[al:My Album]\n"
            "[au:Songwriter]\n"
            "[00:01.00]Line 1\n";
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.metadata.value(QStringLiteral("ar")), QStringLiteral("Test Artist"));
        QCOMPARE(doc.metadata.value(QStringLiteral("ti")), QStringLiteral("My Song"));
        QCOMPARE(doc.metadata.value(QStringLiteral("al")), QStringLiteral("My Album"));
        QCOMPARE(doc.metadata.value(QStringLiteral("au")), QStringLiteral("Songwriter"));
        QCOMPARE(doc.lines.size(), 1);
        QCOMPARE(doc.lines[0].text, QStringLiteral("Line 1"));
    }

    // Criterion 4: offsetMs is applied (timeMs = rawMs - offsetMs); negative times clamped to 0
    void offsetApplication() {
        // offset:200 → rawMs - 200; raw 100ms → -100 → clamped 0; raw 5000ms → 4800ms
        const QString lrc =
            "[offset:200]\n"
            "[00:05.00]Late line\n"
            "[00:00.10]Early line\n";  // raw = 100ms → 100-200 = -100 → 0
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.offsetMs, 200);
        QCOMPARE(doc.lines.size(), 2);
        // Sorted by timeMs after offset:
        QCOMPARE(doc.lines[0].timeMs, 0);
        QCOMPARE(doc.lines[0].text,   QStringLiteral("Early line"));
        QCOMPARE(doc.lines[1].timeMs, 4800);
        QCOMPARE(doc.lines[1].text,   QStringLiteral("Late line"));
    }

    // Criterion 4b: negative offset shifts lyrics later (adds ms to timestamps)
    void negativeOffset() {
        const QString lrc =
            "[offset:-500]\n"
            "[00:01.00]Line\n";  // raw 1000ms, offset -500 → 1000 - (-500) = 1500ms
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.offsetMs, -500);
        QVERIFY(!doc.lines.isEmpty());
        QCOMPARE(doc.lines[0].timeMs, 1500);
    }

    // Criterion 5: output lines are sorted ascending by timeMs
    void sortedOutput() {
        const QString lrc =
            "[00:30.00]Third\n"
            "[00:10.00]First\n"
            "[00:20.00]Second\n";
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.lines.size(), 3);
        QCOMPARE(doc.lines[0].timeMs, 10000);
        QCOMPARE(doc.lines[0].text,   QStringLiteral("First"));
        QCOMPARE(doc.lines[1].timeMs, 20000);
        QCOMPARE(doc.lines[1].text,   QStringLiteral("Second"));
        QCOMPARE(doc.lines[2].timeMs, 30000);
        QCOMPARE(doc.lines[2].text,   QStringLiteral("Third"));
    }

    // Criterion 6: timed lines with blank text (instrumental gaps) are kept
    void blankTextKept() {
        const QString lrc =
            "[00:01.00]\n"
            "[00:02.00]Text\n";
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.lines.size(), 2);
        QCOMPARE(doc.lines[0].timeMs, 1000);
        QCOMPARE(doc.lines[0].text,   QString());
        QCOMPARE(doc.lines[1].timeMs, 2000);
        QCOMPARE(doc.lines[1].text,   QStringLiteral("Text"));
    }

    // Criterion 7: lines without a valid timestamp and not a metadata tag are ignored
    void nonTimestampLinesIgnored() {
        const QString lrc =
            "This is a comment\n"
            "[00:01.00]Line 1\n"
            "Just some plain text\n"
            "[InvalidTag]\n"
            "[00:02.00]Line 2\n";
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.lines.size(), 2);
        QCOMPARE(doc.lines[0].text, QStringLiteral("Line 1"));
        QCOMPARE(doc.lines[1].text, QStringLiteral("Line 2"));
    }

    // Criterion 8: lineIndexForMs returns correct index or -1
    void lineIndexForMs() {
        LrcDocument doc;
        doc.lines = {{1000, QStringLiteral("A")}, {2000, QStringLiteral("B")}, {3000, QStringLiteral("C")}};

        QCOMPARE(LrcParser::lineIndexForMs(doc,  500), -1);  // before first line
        QCOMPARE(LrcParser::lineIndexForMs(doc, 1000),  0);  // exactly at first
        QCOMPARE(LrcParser::lineIndexForMs(doc, 1500),  0);  // between first and second
        QCOMPARE(LrcParser::lineIndexForMs(doc, 2000),  1);  // exactly at second
        QCOMPARE(LrcParser::lineIndexForMs(doc, 2999),  1);  // just before third
        QCOMPARE(LrcParser::lineIndexForMs(doc, 3000),  2);  // exactly at third
        QCOMPARE(LrcParser::lineIndexForMs(doc, 9999),  2);  // past the end → last
    }

    // Extra: empty string input yields empty document
    void emptyInput() {
        const auto doc = LrcParser::parse(QString());
        QVERIFY(!doc.hasTimedLines());
        QVERIFY(doc.metadata.isEmpty());
        QCOMPARE(doc.offsetMs, 0);
        QCOMPARE(LrcParser::lineIndexForMs(doc, 0), -1);
    }

    // Extra: metadata-only input has no timed lines
    void metadataOnlyInput() {
        const QString lrc =
            "[ar:Artist]\n"
            "[ti:Title]\n";
        const auto doc = LrcParser::parse(lrc);
        QVERIFY(!doc.hasTimedLines());
        QCOMPARE(doc.metadata.value(QStringLiteral("ar")), QStringLiteral("Artist"));
        QCOMPARE(doc.metadata.value(QStringLiteral("ti")), QStringLiteral("Title"));
    }

    // Extra: [00:12.500] must equal 12500ms (millisecond vs centisecond regression)
    void msVsCsRegression() {
        // 3-digit fraction "500" = 500 ms (NOT 500*10 = 5000ms)
        const auto doc = LrcParser::parse(QStringLiteral("[00:12.500]Regression"));
        QVERIFY(!doc.lines.isEmpty());
        QCOMPARE(doc.lines[0].timeMs, 12500);
    }

    // Extra: embedded timestamp in text (not leading) is NOT treated as a timing marker
    void embeddedTimestampIsNotTiming() {
        const QString lrc = QStringLiteral("[00:01.00]Hello [00:05.00] world");
        const auto doc = LrcParser::parse(lrc);
        QCOMPARE(doc.lines.size(), 1);
        QCOMPARE(doc.lines[0].timeMs, 1000);
        QCOMPARE(doc.lines[0].text,   QStringLiteral("Hello [00:05.00] world"));
    }
};

QTEST_MAIN(TestLrcParser)
#include "test_lrc_parser.moc"
