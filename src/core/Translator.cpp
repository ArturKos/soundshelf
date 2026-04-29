#include "soundshelf/core/Translator.hpp"

#include <QCoreApplication>
#include <QLocale>
#include <QLibraryInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcI18n, "soundshelf.i18n")

namespace soundshelf {

Translator& Translator::instance() {
    static Translator t;
    return t;
}

Translator::Translator() = default;

QStringList Translator::supportedLocales() {
    return {
        QStringLiteral("en"),
        QStringLiteral("pl"),
        QStringLiteral("de"),
        QStringLiteral("fr"),
    };
}

QStringList Translator::supportedDisplayNames() {
    return {
        QStringLiteral("English"),
        QStringLiteral("Polski"),
        QStringLiteral("Deutsch"),
        QStringLiteral("Français"),
    };
}

QString Translator::detectSystemLocale() {
    const QString sys = QLocale::system().name();   // np. "pl_PL", "en_US", "de_DE"
    const QString lang = sys.left(2).toLower();
    const auto supported = supportedLocales();
    if (supported.contains(lang)) {
        return lang;
    }
    return QStringLiteral("en");
}

bool Translator::loadLocale(const QString& localeCode) {
    const QString code = localeCode.isEmpty()
        ? detectSystemLocale()
        : localeCode.left(2).toLower();

    auto* app = QCoreApplication::instance();
    if (!app) {
        qCWarning(lcI18n) << "No QCoreApplication — cannot install translator";
        return false;
    }

    // Usuń aktualne
    app->removeTranslator(&m_qtTranslator);
    app->removeTranslator(&m_appTranslator);

    // Najpierw Qt's own (przyciski "OK"/"Anuluj" etc.)
    const QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (m_qtTranslator.load(QStringLiteral("qtbase_%1").arg(code), qtPath)) {
        app->installTranslator(&m_qtTranslator);
    }

    // Nasze tłumaczenie z resources lub z dysku
    bool ok = m_appTranslator.load(QStringLiteral(":/translations/soundshelf_%1.qm").arg(code));
    if (!ok) {
        // Fallback — szukaj w katalogu obok binarki
        ok = m_appTranslator.load(QStringLiteral("translations/soundshelf_%1.qm").arg(code));
    }

    if (ok) {
        app->installTranslator(&m_appTranslator);
        m_currentLocale = code;
        qCInfo(lcI18n) << "Loaded locale:" << code;
        emit localeChanged(code);
        return true;
    }

    if (code == QLatin1String("en")) {
        // EN to język źródłowy — brak .qm to OK, używamy stringów z kodu
        m_currentLocale = QStringLiteral("en");
        emit localeChanged(QStringLiteral("en"));
        return true;
    }

    qCWarning(lcI18n) << "Failed to load locale" << code << "— falling back to EN";
    m_currentLocale = QStringLiteral("en");
    emit localeChanged(QStringLiteral("en"));
    return false;
}

} // namespace soundshelf
