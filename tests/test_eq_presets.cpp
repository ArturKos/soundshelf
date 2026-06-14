#include <QtTest>
#include "soundshelf/core/PlayerEngine.hpp"

using namespace soundshelf;

class TestEqPresets : public QObject {
    Q_OBJECT

private slots:

    void availablePresetsAreBundled() {
        const QStringList p = PlayerEngine::availablePresets();
        QVERIFY(p.contains(QStringLiteral("rock")));
        QVERIFY(p.contains(QStringLiteral("flat")));
        QVERIFY(p.contains(QStringLiteral("jazz")));
        QVERIFY(p.contains(QStringLiteral("bass_boost")));
        QVERIFY(p.size() >= 8);
    }

    void rockPresetLoadsExpectedGains() {
        PlayerEngine pe;   // no mpv init needed for EQ state
        pe.setEqualizerPreset(QStringLiteral("rock"));
        const QVector<double> g = pe.equalizerGains();
        QCOMPARE(g.size(), int(PlayerEngine::EQ_BANDS));
        // From resources/eq_presets/rock.json
        const QVector<double> expected{5, 4, 2, -1, -2, 1, 3, 4, 4, 5};
        for (int i = 0; i < g.size(); ++i)
            QCOMPARE(g[i], expected[i]);
    }

    void flatPresetIsAllZero() {
        PlayerEngine pe;
        pe.setEqualizerPreset(QStringLiteral("rock"));   // dirty it first
        pe.setEqualizerPreset(QStringLiteral("flat"));
        for (double v : pe.equalizerGains())
            QCOMPARE(v, 0.0);
    }

    void displayNameWithSpaceMapsToStem() {
        PlayerEngine pe;
        // "Bass Boost" → bass_boost.json
        pe.setEqualizerPreset(QStringLiteral("Bass Boost"));
        // bass_boost should lift the low bands above zero.
        QVERIFY(pe.equalizerGains().first() > 0.0);
    }

    void unknownPresetLeavesGainsUntouched() {
        PlayerEngine pe;
        pe.setEqualizerPreset(QStringLiteral("rock"));
        const QVector<double> before = pe.equalizerGains();
        pe.setEqualizerPreset(QStringLiteral("does_not_exist"));
        QCOMPARE(pe.equalizerGains(), before);
    }

    void gainsAreClampedToRange() {
        PlayerEngine pe;
        pe.setEqualizerPreset(QStringLiteral("bass_boost"));
        for (double v : pe.equalizerGains()) {
            QVERIFY(v >= -12.0);
            QVERIFY(v <= 12.0);
        }
    }
};

QTEST_GUILESS_MAIN(TestEqPresets)
#include "test_eq_presets.moc"
