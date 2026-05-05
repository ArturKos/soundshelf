#include <QtTest>
#include <QCryptographicHash>
#include "soundshelf/network/LastFmClient.hpp"

using namespace soundshelf;

class TestLastFmSign : public QObject {
    Q_OBJECT

private slots:

    void signsParamsInKeyOrder() {
        // Last.fm signing rule: concatenate keys+values in key-sort order,
        // append the shared secret, MD5 the UTF-8 of the result.
        const QMap<QString, QString> params {
            { QStringLiteral("api_key"),  QStringLiteral("KEY123") },
            { QStringLiteral("artist"),   QStringLiteral("Daft Punk") },
            { QStringLiteral("method"),   QStringLiteral("track.scrobble") },
            { QStringLiteral("track"),    QStringLiteral("Around The World") },
            { QStringLiteral("timestamp"),QStringLiteral("1700000000") },
        };
        const QString secret = QStringLiteral("s3cr3t");

        // Compute the expected hash by hand to lock in the contract.
        QString concat;
        // QMap iterates in key order: api_key, artist, method, timestamp, track.
        concat += "api_key" "KEY123";
        concat += "artist"  "Daft Punk";
        concat += "method"  "track.scrobble";
        concat += "timestamp" "1700000000";
        concat += "track"   "Around The World";
        concat += secret;
        const QString expected = QString::fromUtf8(
            QCryptographicHash::hash(concat.toUtf8(),
                                     QCryptographicHash::Md5).toHex());

        QCOMPARE(LastFmClient::signParams(params, secret), expected);
    }

    void signatureChangesWithSecret() {
        const QMap<QString, QString> params{ { QStringLiteral("a"), QStringLiteral("1") } };
        const QString sig1 = LastFmClient::signParams(params, QStringLiteral("aaa"));
        const QString sig2 = LastFmClient::signParams(params, QStringLiteral("bbb"));
        QVERIFY(sig1 != sig2);
        // Hash output is hex(md5) → exactly 32 chars.
        QCOMPARE(sig1.size(), 32);
        QCOMPARE(sig2.size(), 32);
    }

    void signatureChangesWithAnyParam() {
        const QString secret = QStringLiteral("s");
        QMap<QString, QString> p{
            { QStringLiteral("track"),  QStringLiteral("Foo") },
            { QStringLiteral("artist"), QStringLiteral("Bar") },
        };
        const QString a = LastFmClient::signParams(p, secret);
        p[QStringLiteral("track")] = QStringLiteral("Foo!");
        const QString b = LastFmClient::signParams(p, secret);
        QVERIFY(a != b);
    }

    void emptyParamsStillProducesValidHash() {
        const QString sig = LastFmClient::signParams({}, QStringLiteral("only-secret"));
        QCOMPARE(sig.size(), 32);
        // hex chars only.
        for (QChar c : sig) {
            QVERIFY((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
                 || (c >= QLatin1Char('a') && c <= QLatin1Char('f')));
        }
    }
};

QTEST_APPLESS_MAIN(TestLastFmSign)
#include "test_lastfm_sign.moc"
