// src/main.cpp — entry point dla GUI binarka soundshelf

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QLoggingCategory>
#include <QDir>
#include <QStandardPaths>

#include "soundshelf/core/Translator.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/ui/MainWindow.hpp"
#include "soundshelf/ui/ThemeManager.hpp"

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
        // TODO: uruchom HttpServer i nie pokazuj GUI
        qInfo() << "Headless mode not yet wired in. TODO: connect to network::HttpServer";
        return 0;
    }

    // ----- Theme -----
    soundshelf::ThemeManager::instance().applyTheme(
        parser.isSet(themeOpt) ? parser.value(themeOpt) : QStringLiteral("modern_dark"));

    // ----- Main window -----
    soundshelf::MainWindow window;
    window.show();

    return app.exec();
}
