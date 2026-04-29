#include <QtTest>
#include <QCoreApplication>
#include "soundshelf/core/Translator.hpp"

using namespace soundshelf;

class TestTranslator : public QObject {
    Q_OBJECT

private slots:

    void supportedLocales() {
        const auto locales = Translator::supportedLocales();
        QVERIFY(locales.contains("en"));
        QVERIFY(locales.contains("pl"));
        QVERIFY(locales.contains("de"));
        QVERIFY(locales.contains("fr"));
    }

    void displayNamesMatchLocales() {
        QCOMPARE(Translator::supportedLocales().size(),
                 Translator::supportedDisplayNames().size());
    }

    void detectFallsBackToEnglish() {
        // System locale może być cokolwiek — ważne, że zwracamy z listy obsługiwanych
        const QString detected = Translator::detectSystemLocale();
        QVERIFY(Translator::supportedLocales().contains(detected));
    }

    void loadEnglishAlwaysSucceeds() {
        // EN to język źródłowy — load powinien się udać nawet bez .qm
        QVERIFY(Translator::instance().loadLocale("en"));
        QCOMPARE(Translator::instance().currentLocale(), QStringLiteral("en"));
    }

    void localeChangedSignal() {
        QSignalSpy spy(&Translator::instance(), &Translator::localeChanged);
        Translator::instance().loadLocale("en");
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestTranslator)
#include "test_translator.moc"
