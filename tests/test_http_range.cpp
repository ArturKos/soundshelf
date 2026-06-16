#include <QTest>
#include "soundshelf/network/HttpRange.hpp"

using namespace soundshelf;

class TestHttpRange : public QObject {
    Q_OBJECT

private slots:
    // --- parse(): absent / empty header ---
    void parse_emptyHeader_returnsNone()
    {
        const auto r = HttpRange::parse({}, 1000);
        QCOMPARE(r.status, RangeStatus::None);
    }

    // --- parse(): full explicit range ---
    void parse_explicitRange_returnsSatisfiable()
    {
        const auto r = HttpRange::parse("bytes=0-499", 1000);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 0);
        QCOMPARE(r.range.end, 499);
        QCOMPARE(r.range.length(), 500);
    }

    // --- parse(): open-end form ---
    void parse_openEnd_returnsSatisfiable()
    {
        const auto r = HttpRange::parse("bytes=500-", 1000);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 500);
        QCOMPARE(r.range.end, 999);
        QCOMPARE(r.range.length(), 500);
    }

    // --- parse(): suffix form ---
    void parse_suffixRange_returnsSatisfiable()
    {
        const auto r = HttpRange::parse("bytes=-500", 1000);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 500);
        QCOMPARE(r.range.end, 999);
        QCOMPARE(r.range.length(), 500);
    }

    // --- parse(): suffix >= totalSize clamps start to 0 ---
    void parse_suffixLargerThanFile_clampedToStart()
    {
        const auto r = HttpRange::parse("bytes=-500", 200);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 0);
        QCOMPARE(r.range.end, 199);
        QCOMPARE(r.range.length(), 200);
    }

    // --- parse(): open end whole file ---
    void parse_wholeFile_returnsSatisfiable()
    {
        const auto r = HttpRange::parse("bytes=0-", 1000);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 0);
        QCOMPARE(r.range.end, 999);
    }

    // --- parse(): explicit end beyond EOF is clamped ---
    void parse_endBeyondEof_isClamped()
    {
        const auto r = HttpRange::parse("bytes=0-9999", 100);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 0);
        QCOMPARE(r.range.end, 99);
    }

    // --- parse(): start == end (single byte) ---
    void parse_singleByte_returnsSatisfiable()
    {
        const auto r = HttpRange::parse("bytes=0-0", 100);
        QCOMPARE(r.status, RangeStatus::Satisfiable);
        QCOMPARE(r.range.start, 0);
        QCOMPARE(r.range.end, 0);
        QCOMPARE(r.range.length(), 1);
    }

    // --- parse(): start at EOF → Unsatisfiable ---
    void parse_startAtEof_returnsUnsatisfiable()
    {
        const auto r = HttpRange::parse("bytes=1000-", 1000);
        QCOMPARE(r.status, RangeStatus::Unsatisfiable);
    }

    // --- parse(): start beyond EOF with explicit end → Unsatisfiable ---
    void parse_startBeyondEofExplicit_returnsUnsatisfiable()
    {
        const auto r = HttpRange::parse("bytes=1000-2000", 1000);
        QCOMPARE(r.status, RangeStatus::Unsatisfiable);
    }

    // --- parse(): empty file → Unsatisfiable for any range ---
    void parse_emptyFile_explicit_returnsUnsatisfiable()
    {
        const auto r = HttpRange::parse("bytes=0-0", 0);
        QCOMPARE(r.status, RangeStatus::Unsatisfiable);
    }

    void parse_emptyFile_openEnd_returnsUnsatisfiable()
    {
        const auto r = HttpRange::parse("bytes=0-", 0);
        QCOMPARE(r.status, RangeStatus::Unsatisfiable);
    }

    void parse_emptyFile_suffix_returnsUnsatisfiable()
    {
        const auto r = HttpRange::parse("bytes=-100", 0);
        QCOMPARE(r.status, RangeStatus::Unsatisfiable);
    }

    // --- parse(): Malformed cases ---
    void parse_multiRange_returnsMalformed()
    {
        const auto r = HttpRange::parse("bytes=0-1,5-6", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    void parse_nonBytesUnit_returnsMalformed()
    {
        const auto r = HttpRange::parse("foobar=0-499", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    void parse_nonNumericStart_returnsMalformed()
    {
        const auto r = HttpRange::parse("bytes=abc-def", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    void parse_nonNumericEnd_returnsMalformed()
    {
        const auto r = HttpRange::parse("bytes=0-def", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    void parse_startGreaterThanEnd_returnsMalformed()
    {
        const auto r = HttpRange::parse("bytes=500-100", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    void parse_missingDash_returnsMalformed()
    {
        const auto r = HttpRange::parse("bytes=0", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    void parse_noBytesPrefix_returnsMalformed()
    {
        const auto r = HttpRange::parse("0-499", 1000);
        QCOMPARE(r.status, RangeStatus::Malformed);
    }

    // --- contentRange() formatting ---
    void contentRange_formatsCorrectly()
    {
        const ByteRange br{0, 499};
        QCOMPARE(HttpRange::contentRange(br, 1234), QByteArray("bytes 0-499/1234"));
    }

    void contentRange_singleByte_formatsCorrectly()
    {
        const ByteRange br{42, 42};
        QCOMPARE(HttpRange::contentRange(br, 100), QByteArray("bytes 42-42/100"));
    }

    // --- unsatisfiedContentRange() formatting ---
    void unsatisfiedContentRange_formatsCorrectly()
    {
        QCOMPARE(HttpRange::unsatisfiedContentRange(1234), QByteArray("bytes */1234"));
    }

    void unsatisfiedContentRange_zeroSize_formatsCorrectly()
    {
        QCOMPARE(HttpRange::unsatisfiedContentRange(0), QByteArray("bytes */0"));
    }

    // --- ByteRange::length() sanity ---
    void byteRange_length_isCorrect()
    {
        const ByteRange br{100, 199};
        QCOMPARE(br.length(), static_cast<qint64>(100));
    }
};

QTEST_MAIN(TestHttpRange)
#include "test_http_range.moc"
