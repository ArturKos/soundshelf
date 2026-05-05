#include <QtTest>
#include "soundshelf/data/FTS5Index.hpp"

using namespace soundshelf;

class TestFts5Query : public QObject {
    Q_OBJECT

private slots:

    void emptyInputIsEmpty() {
        QCOMPARE(FTS5Index::prepareQuery(QString()),                QString());
        QCOMPARE(FTS5Index::prepareQuery(QStringLiteral("   ")),    QString());
    }

    void singleTokenGetsPrefixStar() {
        // "kraft" → '"kraft"*'  so the user's partial typing matches
        // Kraftwerk, Kraftwave etc.
        QCOMPARE(FTS5Index::prepareQuery(QStringLiteral("kraft")),
                 QStringLiteral("\"kraft\"*"));
    }

    void multiTokenWrapsAllAndStarsLast() {
        // "the dark" → '"the" "dark"*' — first phrase exact, last prefix.
        QCOMPARE(FTS5Index::prepareQuery(QStringLiteral("the dark")),
                 QStringLiteral("\"the\" \"dark\"*"));
    }

    void stripsFtsSyntaxChars() {
        // "  Quotes\"  And:colons (parens)  " — should still produce
        // a valid MATCH expression with no remaining special chars.
        const QString q = FTS5Index::prepareQuery(
            QStringLiteral("  Quotes\"  And:colons (parens)  "));
        // No unescaped " inside the inner tokens, no unbalanced quotes.
        // Format is: "Quotes" "Andcolons" "parens"*
        QVERIFY(!q.contains(QStringLiteral("\"\"\"")));
        QVERIFY(!q.contains(QLatin1Char(':')));
        QVERIFY(!q.contains(QLatin1Char('(')));
        QVERIFY(!q.contains(QLatin1Char(')')));
        QVERIFY(q.endsWith(QStringLiteral("*")));
    }

    void collapsesWhitespaceRuns() {
        // Multiple spaces between tokens should not produce empty tokens.
        const QString q = FTS5Index::prepareQuery(QStringLiteral("a    b\tc"));
        QCOMPARE(q, QStringLiteral("\"a\" \"b\" \"c\"*"));
    }

    void preservesUnicode() {
        // Diacritics should pass through — the FTS5 tokenizer handles
        // them via 'unicode61 remove_diacritics 2'.
        const QString q = FTS5Index::prepareQuery(QStringLiteral("Oxygène"));
        QCOMPARE(q, QStringLiteral("\"Oxygène\"*"));
    }
};

QTEST_APPLESS_MAIN(TestFts5Query)
#include "test_fts5_query.moc"
