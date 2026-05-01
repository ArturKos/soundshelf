#include "soundshelf/ui/ThemeManager.hpp"

#include <QApplication>
#include <QFile>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTheme, "soundshelf.theme")

namespace soundshelf {

ThemeManager& ThemeManager::instance() {
    static ThemeManager t;
    return t;
}

QStringList ThemeManager::availableThemes() {
    return {
        QStringLiteral("modern_dark"),
        QStringLiteral("amber_crt"),
        QStringLiteral("phosphor"),
        QStringLiteral("light"),
    };
}

bool ThemeManager::applyTheme(const QString& name) {
    QFile f(QStringLiteral(":/resources/themes/%1.qss").arg(name));
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(lcTheme) << "Cannot load theme:" << name;
        return false;
    }
    qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    m_currentTheme = name;
    qCInfo(lcTheme) << "Applied theme:" << name;
    emit themeChanged(name);
    return true;
}

} // namespace soundshelf
