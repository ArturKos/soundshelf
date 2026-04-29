#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QStringList>

namespace soundshelf {

/// Ładuje odpowiedni .qm w runtime, emituje sygnał gdy język się zmienia.
/// Singletonowy — instancja przez Translator::instance().
class Translator : public QObject {
    Q_OBJECT
public:
    static Translator& instance();

    /// Lista wspieranych języków: en, pl, de, fr.
    static QStringList supportedLocales();

    /// Lista czytelnych nazw: English, Polski, Deutsch, Français.
    static QStringList supportedDisplayNames();

    /// Aktualnie aktywny kod (np. "pl_PL", "en_US", lub "en").
    QString currentLocale() const { return m_currentLocale; }

    /// Wczytuje i instaluje translator dla danego kodu.
    /// Zwraca true jeśli się udało; false oznacza fallback na EN.
    /// Emituje localeChanged.
    bool loadLocale(const QString& localeCode);

    /// Detekcja systemowa, użyta przy pierwszym starcie.
    /// Zwraca jeden z supportedLocales() lub "en".
    static QString detectSystemLocale();

signals:
    void localeChanged(const QString& newLocale);

private:
    Translator();
    QTranslator m_qtTranslator;        ///< Qt's own translations (qtbase_pl.qm itp.)
    QTranslator m_appTranslator;       ///< Nasze soundshelf_pl.qm
    QString m_currentLocale = QStringLiteral("en");
};

} // namespace soundshelf
