#include <QtTest>
#include "soundshelf/core/PlayerEngine.hpp"

using namespace soundshelf;

class TestAudioFilterChain : public QObject {
    Q_OBJECT

private slots:

    /// (a) EQ disabled and RG zero → no filters → empty string.
    void disabledEqAndNoRg_returnsEmpty() {
        const QVector<double> gains(PlayerEngine::EQ_BANDS, 0.0);
        QCOMPARE(PlayerEngine::buildAudioFilterChain(false, gains, 0.0), QString{});
    }

    /// (b) EQ enabled, only band 0 non-zero (+5 dB at 60 Hz) → exact single-term lavfi.
    void enabledEqSingleNonZeroBand_exactString() {
        QVector<double> gains(PlayerEngine::EQ_BANDS, 0.0);
        gains[0] = 5.0;
        const QString result = PlayerEngine::buildAudioFilterChain(true, gains, 0.0);
        QCOMPARE(result,
                 QStringLiteral("lavfi=[equalizer=f=60:width_type=q:width=1.0:gain=5.0]"));
    }

    /// (c) Rock preset gains {5,4,2,-1,-2,1,3,4,4,5}: all 10 non-zero bands appear
    ///     inside a single lavfi bracket in EQ_FREQS order.
    void rockPresetAllTenBandsPresent() {
        const QVector<double> gains{5, 4, 2, -1, -2, 1, 3, 4, 4, 5};
        const QString result = PlayerEngine::buildAudioFilterChain(true, gains, 0.0);
        QVERIFY(result.startsWith(QStringLiteral("lavfi=[")));
        QVERIFY(result.endsWith(QChar(']')));
        QCOMPARE(result.count(QStringLiteral("equalizer=")), 10);
        for (int i = 0; i < PlayerEngine::EQ_BANDS; ++i) {
            QVERIFY(result.contains(
                QStringLiteral("f=%1").arg(static_cast<int>(PlayerEngine::EQ_FREQS[i]))));
        }
    }

    /// (d) RG only (EQ disabled, replayGainDb = -6.5) → contains volume=-6.50dB.
    void replayGainOnly_containsVolumeTag() {
        const QVector<double> gains(PlayerEngine::EQ_BANDS, 0.0);
        const QString result = PlayerEngine::buildAudioFilterChain(false, gains, -6.5);
        QVERIFY(result.contains(QStringLiteral("volume=-6.50dB")));
    }

    /// (e) EQ + RG combined → both equalizer term(s) and the volume term present.
    void eqAndRgCombined_bothTermsPresent() {
        QVector<double> gains(PlayerEngine::EQ_BANDS, 0.0);
        gains[0] = 3.0;
        const QString result = PlayerEngine::buildAudioFilterChain(true, gains, -3.0);
        QVERIFY(result.startsWith(QStringLiteral("lavfi=[")));
        QVERIFY(result.contains(QStringLiteral("equalizer=")));
        QVERIFY(result.contains(QStringLiteral("volume=-3.00dB")));
    }

    /// EQ enabled but all gains zero → same as disabled → empty.
    void enabledEqAllZeroGains_returnsEmpty() {
        const QVector<double> gains(PlayerEngine::EQ_BANDS, 0.0);
        QCOMPARE(PlayerEngine::buildAudioFilterChain(true, gains, 0.0), QString{});
    }

    /// Single positive RG gain.
    void replayGainPositive_formatCorrect() {
        const QVector<double> gains(PlayerEngine::EQ_BANDS, 0.0);
        const QString result = PlayerEngine::buildAudioFilterChain(false, gains, 2.0);
        QCOMPARE(result, QStringLiteral("lavfi=[volume=2.00dB]"));
    }
};

QTEST_GUILESS_MAIN(TestAudioFilterChain)
#include "test_audio_filter_chain.moc"
