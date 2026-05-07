// src/main.cpp — entry point dla GUI binarka soundshelf

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QLoggingCategory>
#include <QDir>
#include <QStandardPaths>

#include "soundshelf/core/Translator.hpp"
#include "soundshelf/core/SettingsManager.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/network/HttpServer.hpp"
#include "soundshelf/ui/MainWindow.hpp"
#include "soundshelf/ui/ThemeManager.hpp"

#include <QHostAddress>
#include <QUuid>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SoundShelf"));
    QApplication::setApplicationDisplayName(QStringLiteral("SoundShelf"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QApplication::setOrganizationName(QStringLiteral("SoundShelf"));
    QApplication::setOrganizationDomain(QStringLiteral("soundshelf.example.com"));

    // ----- CLI args -----
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SoundShelf — audio catalog and player"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption langOpt({"l", "lang"},
        QStringLiteral("Interface language (en, pl, de, fr)"), QStringLiteral("code"));
    QCommandLineOption dbOpt({"d", "db"},
        QStringLiteral("Database path"), QStringLiteral("path"));
    QCommandLineOption themeOpt({"t", "theme"},
        QStringLiteral("Theme (modern_dark, amber_crt, phosphor, light)"),
        QStringLiteral("name"));
    QCommandLineOption serveOpt(QStringLiteral("serve"),
        QStringLiteral("Run as headless HTTP server (no GUI)"));
    QCommandLineOption portOpt(QStringLiteral("port"),
        QStringLiteral("HTTP server port"), QStringLiteral("port"), "8080");

    parser.addOption(langOpt);
    parser.addOption(dbOpt);
    parser.addOption(themeOpt);
    parser.addOption(serveOpt);
    parser.addOption(portOpt);
    parser.process(app);

    // ----- i18n -----
    auto& tr = soundshelf::Translator::instance();
    tr.loadLocale(parser.value(langOpt));

    // ----- Database -----
    auto& dbMgr = soundshelf::DatabaseManager::instance();
    const QString dbPath = parser.isSet(dbOpt)
        ? parser.value(dbOpt)
        : soundshelf::DatabaseManager::defaultDbPath();
    auto dbResult = dbMgr.open(dbPath);
    if (!dbResult) {
        qCritical() << "Cannot open database:" << dbResult.error().message;
        return 2;
    }

    // ----- Headless mode? -----
    if (parser.isSet(serveOpt)) {
#ifdef SOUNDSHELF_HAVE_HTTPSERVER
        // Bearer token: persist across restarts, generate one on first
        // serve. Stored via SettingsManager so the user can also paste
        // it into Preferences → Network for clients.
        auto& settings = soundshelf::SettingsManager::instance();
        auto& dbm = soundshelf::DatabaseManager::instance();
        auto tokR = dbm.getSetting(QStringLiteral("server.bearer_token"));
        QString token = tokR.isOk() ? tokR.value() : QString();
        if (token.isEmpty()) {
            token = QUuid::createUuid().toString(QUuid::WithoutBraces);
            dbm.setSetting(QStringLiteral("server.bearer_token"), token);
        }
        Q_UNUSED(settings);

        soundshelf::HttpServer server;
        server.setBearerToken(token);
        const quint16 port = parser.value(portOpt).toUShort();
        auto r = server.start(QHostAddress::Any, port ? port : 8080);
        if (!r) {
            qCritical() << "Cannot start HTTP server:" << r.error().message;
            return 3;
        }
        qInfo().noquote() << QStringLiteral("SoundShelf serving on port %1")
                                 .arg(port ? port : 8080);
        qInfo().noquote() << QStringLiteral("Bearer token: %1").arg(token);
        return app.exec();
#else
        qCritical() << "Built without QHttpServer — --serve not available.";
        return 4;
#endif
    }

    // ----- Theme -----
    soundshelf::ThemeManager::instance().applyTheme(
        parser.isSet(themeOpt) ? parser.value(themeOpt) : QStringLiteral("modern_dark"));

    // ----- Main window -----
    soundshelf::MainWindow window;
    window.show();

    return app.exec();
}
