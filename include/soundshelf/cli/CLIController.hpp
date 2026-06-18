#pragma once

#include <QCommandLineParser>
#include <QStringList>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/PodcastManager.hpp"

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

    /**
     * @brief Injects a custom feed/enclosure fetcher used by cmdPodcast().
     *
     * When set, this fetcher is passed to both PodcastManager::setFeedFetcher()
     * and PodcastManager::setEnclosureFetcher() before any subscribe, refresh,
     * or download operation is performed.  Intended for unit tests that must
     * operate without a live network connection.
     *
     * @param fetcher  Synchronous byte-fetcher to inject; pass a default-constructed
     *                 std::function to clear a previously injected stub.
     */
    void setPodcastFetcherForTesting(PodcastManager::FeedFetcher fetcher);

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
    /**
     * @brief Handles the `podcast` subcommand group (feature #12).
     *
     * Delegates to core::PodcastManager and data::PodcastStore.  Subcommands:
     * list, subscribe, refresh, episodes, download, played, unsubscribe.
     *
     * @param args  Arguments after the `podcast` token (subcommand + its args).
     * @return 0 on success, 1 on usage error, 2 on runtime error.
     */
    int cmdPodcast(const QStringList& args);
    int cmdRemote(const QStringList& args);
    int cmdServe(const QStringList& args);
    int cmdDaemon(const QStringList& args);
    int cmdScrobble(const QStringList& args);
    int cmdPlugin(const QStringList& args);
    int cmdStats(const QStringList& args);
    int cmdExport(const QStringList& args);
    int cmdDb(const QStringList& args);
    /**
     * @brief Handles the `tags sync` subcommand (feature D7).
     *
     * Fills missing or placeholder tag fields (title, artist, album) for
     * tracks in the library.  Resolution is attempted via AcoustID/MusicBrainz
     * (when an API key is configured) and falls back to filename parsing.
     *
     * @param args  Arguments after the `tags` token (e.g. `sync --all`).
     * @return 0 on success, 1 on usage error, 2 on runtime error.
     */
    int cmdTagsSync(const QStringList& args);
    int cmdHelp();
    int cmdVersion();

    QString m_locale;
    QString m_format = QStringLiteral("table");   // json / table / csv
    QString m_dbPath;
    QString m_remoteServer;   ///< Set by --server global flag; used by cmdRemote.
    QString m_remoteToken;    ///< Set by --token global flag; used by cmdRemote.
    bool m_quiet = false;
    bool m_verbose = false;

    PodcastManager::FeedFetcher m_podcastFetcher; ///< Injected stub for tests; empty = use default network.

    /// Lazy init
    DatabaseManager* db();
    PlayerEngine* player();
    PlayerEngine* m_player = nullptr;
};

} // namespace soundshelf
