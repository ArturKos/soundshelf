#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

#include "soundshelf/network/RemoteClient.hpp"

using namespace soundshelf;

class TestRemoteClient : public QObject {
    Q_OBJECT

private slots:

    // ── buildListQuery ──────────────────────────────────────────────────────

    void buildListQuery_allParams_allPresentInQuery()
    {
        const auto q = RemoteClient::buildListQuery(
            QStringLiteral("kraftwerk"), 25, QStringLiteral("mytoken"));
        QCOMPARE(q.queryItemValue(QStringLiteral("q")),     QStringLiteral("kraftwerk"));
        QCOMPARE(q.queryItemValue(QStringLiteral("limit")), QStringLiteral("25"));
        QCOMPARE(q.queryItemValue(QStringLiteral("token")), QStringLiteral("mytoken"));
    }

    void buildListQuery_emptyQuery_omitsQ()
    {
        const auto q = RemoteClient::buildListQuery({}, 10, QStringLiteral("tok"));
        QVERIFY(!q.hasQueryItem(QStringLiteral("q")));
        QVERIFY(q.hasQueryItem(QStringLiteral("limit")));
        QVERIFY(q.hasQueryItem(QStringLiteral("token")));
    }

    void buildListQuery_zeroLimit_omitsLimit()
    {
        const auto q = RemoteClient::buildListQuery(QStringLiteral("test"), 0, QStringLiteral("tok"));
        QVERIFY(!q.hasQueryItem(QStringLiteral("limit")));
    }

    void buildListQuery_negativeLimit_omitsLimit()
    {
        const auto q = RemoteClient::buildListQuery(QStringLiteral("test"), -5, QStringLiteral("tok"));
        QVERIFY(!q.hasQueryItem(QStringLiteral("limit")));
    }

    void buildListQuery_emptyToken_omitsToken()
    {
        const auto q = RemoteClient::buildListQuery(QStringLiteral("test"), 10, {});
        QVERIFY(!q.hasQueryItem(QStringLiteral("token")));
    }

    void buildListQuery_specialChars_percentEncoded()
    {
        // Values with spaces and special chars must be percent-encoded in the URL.
        const auto q = RemoteClient::buildListQuery(
            QStringLiteral("jazz & blues"), 5, QStringLiteral("my secret token"));

        // QUrlQuery stores values decoded; they're encoded when serialised to a URL.
        QCOMPARE(q.queryItemValue(QStringLiteral("q")),     QStringLiteral("jazz & blues"));
        QCOMPARE(q.queryItemValue(QStringLiteral("token")), QStringLiteral("my secret token"));

        // When serialised, spaces must not appear as literal spaces.
        QUrl u;
        u.setQuery(q);
        const QByteArray encoded = u.toEncoded();
        QVERIFY(!encoded.contains(QByteArray(1, ' ')));
    }

    // ── trackFromJson ───────────────────────────────────────────────────────

    void trackFromJson_allFields_mapped()
    {
        QJsonObject o;
        o[QStringLiteral("id")]          = 42;
        o[QStringLiteral("title")]       = QStringLiteral("Oxygène Part I");
        o[QStringLiteral("artist")]      = QStringLiteral("Jean-Michel Jarre");
        o[QStringLiteral("album")]       = QStringLiteral("Oxygène");
        o[QStringLiteral("duration_ms")] = qint64(368000);
        o[QStringLiteral("track_no")]    = 1;
        o[QStringLiteral("disc_no")]     = 2;
        o[QStringLiteral("path")]        = QStringLiteral("/music/oxygene/01.flac");

        const RemoteTrack t = RemoteClient::trackFromJson(o);
        QCOMPARE(t.id,          42);
        QCOMPARE(t.title,       QStringLiteral("Oxygène Part I"));
        QCOMPARE(t.artist,      QStringLiteral("Jean-Michel Jarre"));
        QCOMPARE(t.album,       QStringLiteral("Oxygène"));
        QCOMPARE(t.durationMs,  qint64(368000));
        QCOMPARE(t.trackNumber, 1);
        QCOMPARE(t.discNumber,  2);
        QCOMPARE(t.filepath,    QStringLiteral("/music/oxygene/01.flac"));
    }

    void trackFromJson_missingFields_defaultValues()
    {
        const RemoteTrack t = RemoteClient::trackFromJson({});
        QCOMPARE(t.id,          0);
        QCOMPARE(t.title,       QString());
        QCOMPARE(t.artist,      QString());
        QCOMPARE(t.album,       QString());
        QCOMPARE(t.durationMs,  qint64(0));
        QCOMPARE(t.trackNumber, 0);
        QCOMPARE(t.discNumber,  0);
        QCOMPARE(t.filepath,    QString());
    }

    // ── parseTrackList ──────────────────────────────────────────────────────

    void parseTrackList_validArray_mapsAllFields()
    {
        QJsonObject o;
        o[QStringLiteral("id")]          = 7;
        o[QStringLiteral("title")]       = QStringLiteral("Autobahn");
        o[QStringLiteral("artist")]      = QStringLiteral("Kraftwerk");
        o[QStringLiteral("album")]       = QStringLiteral("Autobahn");
        o[QStringLiteral("duration_ms")] = qint64(1396000);
        o[QStringLiteral("track_no")]    = 1;
        o[QStringLiteral("disc_no")]     = 1;
        o[QStringLiteral("path")]        = QStringLiteral("/music/autobahn.flac");

        const QByteArray json =
            QJsonDocument(QJsonArray{o}).toJson(QJsonDocument::Compact);
        const auto r = RemoteClient::parseTrackList(json);
        QVERIFY(r);
        QCOMPARE(r.value().size(), 1);
        const RemoteTrack& t = r.value().constFirst();
        QCOMPARE(t.id,          7);
        QCOMPARE(t.title,       QStringLiteral("Autobahn"));
        QCOMPARE(t.artist,      QStringLiteral("Kraftwerk"));
        QCOMPARE(t.album,       QStringLiteral("Autobahn"));
        QCOMPARE(t.durationMs,  qint64(1396000));
        QCOMPARE(t.trackNumber, 1);
        QCOMPARE(t.discNumber,  1);
        QCOMPARE(t.filepath,    QStringLiteral("/music/autobahn.flac"));
    }

    void parseTrackList_emptyArray_emptyListOk()
    {
        const QByteArray json =
            QJsonDocument(QJsonArray{}).toJson(QJsonDocument::Compact);
        const auto r = RemoteClient::parseTrackList(json);
        QVERIFY(r);
        QVERIFY(r.value().isEmpty());
    }

    void parseTrackList_nonArrayJson_returnsError()
    {
        const auto r = RemoteClient::parseTrackList(R"({"key":"value"})");
        QVERIFY(!r);
    }

    void parseTrackList_malformedBytes_returnsError()
    {
        const auto r = RemoteClient::parseTrackList("not json at all }{");
        QVERIFY(!r);
    }

    void parseTrackList_emptyBytes_returnsError()
    {
        const auto r = RemoteClient::parseTrackList({});
        QVERIFY(!r);
    }

    void parseTrackList_nullJson_returnsError()
    {
        const auto r = RemoteClient::parseTrackList("null");
        QVERIFY(!r);
    }

    // ── parseTrack ──────────────────────────────────────────────────────────

    void parseTrack_validObject_allFields()
    {
        QJsonObject o;
        o[QStringLiteral("id")]          = 99;
        o[QStringLiteral("title")]       = QStringLiteral("Oxygène Part II");
        o[QStringLiteral("artist")]      = QStringLiteral("Jean-Michel Jarre");
        o[QStringLiteral("album")]       = QStringLiteral("Oxygène");
        o[QStringLiteral("duration_ms")] = qint64(243000);
        o[QStringLiteral("track_no")]    = 2;
        o[QStringLiteral("disc_no")]     = 1;
        o[QStringLiteral("path")]        = QStringLiteral("/music/02.flac");

        const QByteArray json = QJsonDocument(o).toJson(QJsonDocument::Compact);
        const auto r = RemoteClient::parseTrack(json);
        QVERIFY(r);
        QCOMPARE(r.value().id,          99);
        QCOMPARE(r.value().title,       QStringLiteral("Oxygène Part II"));
        QCOMPARE(r.value().artist,      QStringLiteral("Jean-Michel Jarre"));
        QCOMPARE(r.value().album,       QStringLiteral("Oxygène"));
        QCOMPARE(r.value().durationMs,  qint64(243000));
        QCOMPARE(r.value().trackNumber, 2);
        QCOMPARE(r.value().discNumber,  1);
        QCOMPARE(r.value().filepath,    QStringLiteral("/music/02.flac"));
    }

    void parseTrack_emptyBytes_returnsError()
    {
        const auto r = RemoteClient::parseTrack({});
        QVERIFY(!r);
    }

    void parseTrack_jsonArray_returnsError()
    {
        const auto r = RemoteClient::parseTrack(QByteArray("[{\"id\":1}]"));
        QVERIFY(!r);
    }

    void parseTrack_malformedJson_returnsError()
    {
        const auto r = RemoteClient::parseTrack("{{broken");
        QVERIFY(!r);
    }

    void parseTrack_nullJson_returnsError()
    {
        const auto r = RemoteClient::parseTrack("null");
        QVERIFY(!r);
    }

    // ── streamUrl ───────────────────────────────────────────────────────────

    void streamUrl_hasCorrectFormat()
    {
        RemoteClient c(QStringLiteral("http://192.168.1.10:8080"),
                       QStringLiteral("secret"));
        const QString url = c.streamUrl(42);
        QVERIFY(url.startsWith(
            QStringLiteral("http://192.168.1.10:8080/api/v1/stream/42")));
        QVERIFY(url.contains(QStringLiteral("token=secret")));
    }

    void streamUrl_baseWithTrailingSlash_noDoubleSlash()
    {
        RemoteClient c(QStringLiteral("http://host:8080/"),
                       QStringLiteral("tok"));
        const QString url = c.streamUrl(1);
        QVERIFY(!url.contains(QStringLiteral("//api")));
        QVERIFY(url.contains(QStringLiteral("/api/v1/stream/1")));
    }

    void streamUrl_noToken_omitsQueryParam()
    {
        RemoteClient c(QStringLiteral("http://host:8080"), {});
        const QString url = c.streamUrl(5);
        QVERIFY(!url.contains(QStringLiteral("token=")));
        QVERIFY(url.contains(QStringLiteral("/api/v1/stream/5")));
    }
};

QTEST_GUILESS_MAIN(TestRemoteClient)
#include "test_remote_client.moc"
