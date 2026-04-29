#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "soundshelf/core/SmartPlaylistEvaluator.hpp"

using namespace soundshelf;

class TestSmartPlaylist : public QObject {
    Q_OBJECT

private slots:

    void validateSimpleRule() {
        QJsonObject rules{
            {"match", "all"},
            {"rules", QJsonArray{
                QJsonObject{{"field", "year"}, {"op", "gte"}, {"value", 2020}}
            }}
        };
        QString err;
        QVERIFY(SmartPlaylistEvaluator::validate(rules, &err));
        QVERIFY(err.isEmpty());
    }

    void rejectUnknownField() {
        QJsonObject rules{
            {"rules", QJsonArray{
                QJsonObject{{"field", "spaceships"}, {"op", "eq"}, {"value", 1}}
            }}
        };
        QString err;
        QVERIFY(!SmartPlaylistEvaluator::validate(rules, &err));
        QVERIFY(err.contains("spaceships"));
    }

    void rejectMissingRules() {
        QJsonObject rules{{"match", "all"}};
        QString err;
        QVERIFY(!SmartPlaylistEvaluator::validate(rules, &err));
    }

    void compileNumericGte() {
        QJsonObject rules{
            {"rules", QJsonArray{
                QJsonObject{{"field", "year"}, {"op", "gte"}, {"value", 2020}}
            }}
        };
        auto r = SmartPlaylistEvaluator::compile(rules);
        QVERIFY(r.isOk());
        QVERIFY(r.value().sql.contains("year"));
        QVERIFY(r.value().sql.contains(">="));
        QCOMPARE(r.value().parameters.size(), 1);
        QCOMPARE(r.value().parameters.first().toInt(), 2020);
    }

    void compileStringContains() {
        QJsonObject rules{
            {"rules", QJsonArray{
                QJsonObject{{"field", "title"}, {"op", "contains"}, {"value", "love"}}
            }}
        };
        auto r = SmartPlaylistEvaluator::compile(rules);
        QVERIFY(r.isOk());
        QVERIFY(r.value().sql.contains("LIKE"));
        QCOMPARE(r.value().parameters.first().toString(), QStringLiteral("%love%"));
    }

    void compileBetween() {
        QJsonObject rules{
            {"rules", QJsonArray{
                QJsonObject{{"field", "year"}, {"op", "between"},
                            {"value", QJsonArray{1990, 2000}}}
            }}
        };
        auto r = SmartPlaylistEvaluator::compile(rules);
        QVERIFY(r.isOk());
        QVERIFY(r.value().sql.contains("BETWEEN"));
        QCOMPARE(r.value().parameters.size(), 2);
    }

    void compileMultipleRulesAll() {
        QJsonObject rules{
            {"match", "all"},
            {"rules", QJsonArray{
                QJsonObject{{"field", "year"}, {"op", "gte"}, {"value", 2020}},
                QJsonObject{{"field", "play_count"}, {"op", "gte"}, {"value", 5}}
            }}
        };
        auto r = SmartPlaylistEvaluator::compile(rules);
        QVERIFY(r.isOk());
        QVERIFY(r.value().sql.contains("AND"));
        QCOMPARE(r.value().parameters.size(), 2);
    }

    void compileWithLimitAndOrder() {
        QJsonObject rules{
            {"rules", QJsonArray{
                QJsonObject{{"field", "rating"}, {"op", "gte"}, {"value", 4}}
            }},
            {"limit", 50},
            {"order_by", "random"}
        };
        auto r = SmartPlaylistEvaluator::compile(rules);
        QVERIFY(r.isOk());
        QVERIFY(r.value().sql.contains("ORDER BY RANDOM"));
        QVERIFY(r.value().sql.contains("LIMIT"));
    }

    void compileInList() {
        QJsonObject rules{
            {"rules", QJsonArray{
                QJsonObject{{"field", "format"}, {"op", "in"},
                            {"value", QJsonArray{"FLAC", "MP3"}}}
            }}
        };
        auto r = SmartPlaylistEvaluator::compile(rules);
        QVERIFY(r.isOk());
        QVERIFY(r.value().sql.contains("IN"));
        QCOMPARE(r.value().parameters.size(), 2);
    }

    void supportedFieldsNotEmpty() {
        QVERIFY(!SmartPlaylistEvaluator::supportedFields().isEmpty());
        QVERIFY(SmartPlaylistEvaluator::supportedFields().contains("title"));
    }
};

QTEST_MAIN(TestSmartPlaylist)
#include "test_smart_playlist.moc"
