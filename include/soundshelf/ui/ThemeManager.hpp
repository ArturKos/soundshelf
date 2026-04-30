#pragma once

#include <QObject>
#include <QString>

namespace soundshelf {

/// Zarządza motywami aplikacji. Singleton.
/// Motywy: modern_dark, amber_crt, phosphor, light.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    /// Aplikuje motyw z `resources/themes/&lt;name&gt;.qss`.
    /// Emituje themeChanged.
    bool applyTheme(const QString& name);

    QString currentTheme() const { return m_currentTheme; }
    static QStringList availableThemes();

signals:
    void themeChanged(const QString& name);

private:
    ThemeManager() = default;
    QString m_currentTheme;
};

} // namespace soundshelf
