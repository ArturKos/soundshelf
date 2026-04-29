#pragma once

#include <QCommandLineParser>
#include <QStringList>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

class PlayerEngine;
class DatabaseManager;

/// Parser i dispatcher komend CLI.
/// Wzorzec: subcommand-based (jak git, kubectl).
class CLIController {
public:
    CLIController();

    /// Wykonuje komendę. Zwraca exit code (0 = OK).
    int run(const QStringList& args);

    /// Używane jeśli GUI już chodzi — deleguje przez D-Bus / named pipe.
    /// Zwraca true jeśli udało się skomunikować z istniejącą instancją.
    bool tryDelegate(const QStringList& args);

private:
    // Subcommandy
    int cmdPlay(const QStringList& args);
    int cmdPause();
    int cmdResume();
    int cmdStop();
    int cmdStatus();
    int cmdNext();
    int cmdPrev();
    int cmdSeek(const QStringList& args);
    int cmdVolume(const QStringList& args);
    int cmdImport(const QStringList& args);
    int cmdList(const QStringList& args);
    int cmdSearch(const QStringList& args);
    int cmdInfo(const QStringList& args);
    int cmdTag(const QStringList& args);
    int cmdDisc(const QStringList& args);
    int cmdReplaygain(const QStringList& args);
    int cmdFingerprint(const QStringList& args);
    int cmdConvert(const QStringList& args);
    int cmdDuplicates(const QStringList& args);
    int cmdPlaylist(const QStringList& args);
    int cmdRemote(const QStringList& args);
    int cmdServe(const QStringList& args);
    int cmdDaemon(const QStringList& args);
    int cmdScrobble(const QStringList& args);
    int cmdPlugin(const QStringList& args);
    int cmdStats(const QStringList& args);
    int cmdExport(const QStringList& args);
    int cmdDb(const QStringList& args);
    int cmdHelp();
    int cmdVersion();

    QString m_locale;
    QString m_format = QStringLiteral("table");   // json / table / csv
    QString m_dbPath;
    bool m_quiet = false;
    bool m_verbose = false;

    /// Lazy init
    DatabaseManager* db();
    PlayerEngine* player();
    PlayerEngine* m_player = nullptr;
};

} // namespace soundshelf
