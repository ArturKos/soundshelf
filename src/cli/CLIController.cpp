#include "soundshelf/cli/CLIController.hpp"

#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/core/Translator.hpp"
#include "soundshelf/core/DuplicateDetector.hpp"
#include "soundshelf/core/PlaylistManager.hpp"
#include "soundshelf/core/DiscManager.hpp"
#include "soundshelf/core/PluginManager.hpp"
#include "soundshelf/core/Scrobbler.hpp"
#include "soundshelf/core/ReplayGainAnalyzer.hpp"
#include "soundshelf/core/ChromaprintEngine.hpp"
#include "soundshelf/core/PodcastManager.hpp"
#include "soundshelf/network/AcoustIDClient.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/PlayHistory.hpp"
#include "soundshelf/data/PodcastStore.hpp"
#include "soundshelf/data/SchemaMigrator.hpp"
#include "soundshelf/io/TagInfo.hpp"
#include "soundshelf/io/FormatConverter.hpp"
#include "soundshelf/io/PlaylistExporter.hpp"
#include "soundshelf/io/PlaylistImporter.hpp"
#include "soundshelf/network/HttpServer.hpp"

#include <QCoreApplication>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QStandardPaths>
#include <QUuid>
#include <QSqlQuery>
#include <QSqlError>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCli, "soundshelf.cli")

namespace soundshelf {

namespace {

QTextStream& stdOut() {
    static QTextStream s(stdout);
    return s;
}
QTextStream& stdErr() {
    static QTextStream s(stderr);
    return s;
}

void printUsage() {
    stdOut() << QCoreApplication::translate("CLI",
        "SoundShelf CLI — audio library and player\n"
        "\n"
        "Usage: soundshelf <command> [options]\n"
        "\n"
        "Global flags:\n"
        "  --lang en|pl|de|fr        Interface language\n"
        "  --db PATH                 Database path\n"
        "  --format json|table|csv   Output format\n"
        "  -q, --quiet               Errors only\n"
        "  -v, --verbose             Debug output\n"
        "  -h, --help                Show this help\n"
        "  --version                 Show version\n"
        "\n"
        "Playback:\n"
        "  play <query|id|path>      Play track\n"
        "  pause | resume | stop\n"
        "  next | prev\n"
        "  seek <pos>                +30s, -30s, 1:23\n"
        "  volume <0-100|+5|-5>\n"
        "  status                    Show now playing\n"
        "\n"
        "Library:\n"
        "  import <path>             Recursive import\n"
        "  list [--artist X] [--genre Y] [--year YYYY]\n"
        "  search <query>            Full-text search\n"
        "  info <id>                 Track details\n"
        "\n"
        "Discs:\n"
        "  disc list                 All discs\n"
        "  disc search <query>       Search by disc/album name\n"
        "  disc add --folder <path>\n"
        "  disc add --drive <device>\n"
        "  disc rip <device> [--format flac|mp3|ogg|opus]\n"
        "  disc lookup <device>      MusicBrainz lookup\n"
        "  disc tracks <id>          List tracks of disc\n"
        "  disc play <id>            Play whole disc\n"
        "\n"
        "Tags:\n"
        "  tag show <id|path>\n"
        "  tag set <id|path> --title \"X\" --artist \"Y\"\n"
        "  tag fetch <id> --source musicbrainz|acoustid\n"
        "\n"
        "Other commands:\n"
        "  replaygain <id|path|--all>\n"
        "  fingerprint <id|path|--all>\n"
        "  convert <id|path> --to flac|mp3|ogg|opus\n"
        "  duplicates scan|resolve\n"
        "  playlist list|create|export|import\n"
        "\n"
        "Podcasts:\n"
        "  podcast list                        List subscribed feeds\n"
        "  podcast subscribe <url>             Subscribe to a podcast feed\n"
        "  podcast refresh <id> | --all        Re-fetch feed(s) for new episodes\n"
        "  podcast episodes <feedId>           List episodes for a feed\n"
        "  podcast download <epId> [--dir DIR] Download episode audio file\n"
        "  podcast played <epId> [--unset]     Mark episode played / unplayed\n"
        "  podcast unsubscribe <feedId>        Remove feed and all its episodes\n"
        "\n"
        "  remote list|add|sync|play|search\n"
        "  serve [--port 8080] [--bind 0.0.0.0] [--auth TOKEN]\n"
        "  daemon start|stop|status\n"
        "  scrobble status|flush|auth\n"
        "  plugin list|install|enable|disable|remove\n"
        "  stats top-artists|top-tracks|listening-time|heatmap\n"
        "  export library --format json|csv|xml --out FILE\n"
        "  db vacuum|backup|restore|migrate\n"
        "\n"
        "See https://example.com/soundshelf for full docs.\n").toUtf8().constData();
}

} // anonymous

CLIController::CLIController() = default;

DatabaseManager* CLIController::db() {
    auto& mgr = DatabaseManager::instance();
    if (!mgr.isOpen()) {
        const QString path = m_dbPath.isEmpty()
            ? DatabaseManager::defaultDbPath()
            : m_dbPath;
        auto r = mgr.open(path);
        if (!r) {
            stdErr() << "Cannot open database: " << r.error().message << "\n";
            return nullptr;
        }
    }
    return &mgr;
}

PlayerEngine* CLIController::player() {
    if (!m_player) {
        m_player = new PlayerEngine();
        auto r = m_player->initialize();
        if (!r) {
            stdErr() << "Cannot initialize player: " << r.error().message << "\n";
            delete m_player; m_player = nullptr;
            return nullptr;
        }
    }
    return m_player;
}

int CLIController::run(const QStringList& args) {
    if (args.size() < 2 || args.contains("-h") || args.contains("--help")) {
        return cmdHelp();
    }

    // Parsuj globalne flagi (mogą być przed lub po komendzie)
    QStringList rest;
    for (int i = 1; i < args.size(); ++i) {
        const QString& a = args[i];
        if (a == "--lang" && i + 1 < args.size())          { m_locale = args[++i]; }
        else if (a == "--db" && i + 1 < args.size())       { m_dbPath = args[++i]; }
        else if (a == "--format" && i + 1 < args.size())   { m_format = args[++i]; }
        else if (a == "-q" || a == "--quiet")              { m_quiet = true; }
        else if (a == "-v" || a == "--verbose")            { m_verbose = true; }
        else if (a == "--version")                          { return cmdVersion(); }
        else { rest << a; }
    }

    // Załaduj język
    Translator::instance().loadLocale(m_locale.isEmpty()
        ? Translator::detectSystemLocale()
        : m_locale);

    if (rest.isEmpty()) return cmdHelp();

    const QString& cmd = rest[0];
    const QStringList sub = rest.mid(1);

    if      (cmd == "play")        return cmdPlay(sub);
    else if (cmd == "pause")       return cmdPause();
    else if (cmd == "resume")      return cmdResume();
    else if (cmd == "stop")        return cmdStop();
    else if (cmd == "status")      return cmdStatus();
    else if (cmd == "next")        return cmdNext();
    else if (cmd == "prev")        return cmdPrev();
    else if (cmd == "seek")        return cmdSeek(sub);
    else if (cmd == "volume")      return cmdVolume(sub);
    else if (cmd == "import")      return cmdImport(sub);
    else if (cmd == "list")        return cmdList(sub);
    else if (cmd == "search")      return cmdSearch(sub);
    else if (cmd == "info")        return cmdInfo(sub);
    else if (cmd == "tag")         return cmdTag(sub);
    else if (cmd == "disc")        return cmdDisc(sub);
    else if (cmd == "replaygain")  return cmdReplaygain(sub);
    else if (cmd == "fingerprint") return cmdFingerprint(sub);
    else if (cmd == "convert")     return cmdConvert(sub);
    else if (cmd == "duplicates")  return cmdDuplicates(sub);
    else if (cmd == "playlist")    return cmdPlaylist(sub);
    else if (cmd == "podcast")     return cmdPodcast(sub);
    else if (cmd == "remote")      return cmdRemote(sub);
    else if (cmd == "serve")       return cmdServe(sub);
    else if (cmd == "daemon")      return cmdDaemon(sub);
    else if (cmd == "scrobble")    return cmdScrobble(sub);
    else if (cmd == "plugin")      return cmdPlugin(sub);
    else if (cmd == "stats")       return cmdStats(sub);
    else if (cmd == "export")      return cmdExport(sub);
    else if (cmd == "db")          return cmdDb(sub);
    else if (cmd == "help")        return cmdHelp();
    else {
        stdErr() << "Unknown command: " << cmd << "\n";
        return cmdHelp();
    }
}

int CLIController::cmdHelp() {
    printUsage();
    return 0;
}

int CLIController::cmdVersion() {
    stdOut() << "SoundShelf 0.3.0\n";
    return 0;
}

int CLIController::cmdImport(const QStringList& args) {
    if (args.isEmpty()) {
        stdErr() << QCoreApplication::translate("CLI", "Usage: import <path>") << "\n";
        return 1;
    }
    auto* d = db();
    if (!d) return 2;

    const QString path = args[0];
    QFileInfo fi(path);
    if (!fi.exists()) {
        stdErr() << QCoreApplication::translate("CLI", "Path not found: %1").arg(path) << "\n";
        return 1;
    }

    int imported = 0, skipped = 0, failed = 0;
    QDirIterator it(path,
                    QStringList() << "*.mp3" << "*.flac" << "*.ogg" << "*.opus"
                                  << "*.aac" << "*.m4a" << "*.wav" << "*.ape" << "*.wv",
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        auto r = TagInfo::fromFile(file);
        if (!r) { ++failed; continue; }

        Track t;
        t.filepath = file;
        t.filename = QFileInfo(file).fileName();
        t.format = audioFormatFromFilename(file);
        t.mtime = QFileInfo(file).lastModified();
        r.value().applyToTrack(t);

        auto upsert = d->upsertTrack(t);
        if (upsert) {
            ++imported;
            if (!m_quiet) stdOut() << "+ " << file << "\n";
        } else {
            ++failed;
            stdErr() << "x " << file << ": " << upsert.error().message << "\n";
        }
        Q_UNUSED(skipped);
    }

    if (m_format == "json") {
        QJsonObject o{{"imported", imported}, {"failed", failed}};
        stdOut() << QJsonDocument(o).toJson(QJsonDocument::Compact) << "\n";
    } else {
        stdOut() << QCoreApplication::translate("CLI",
            "Imported: %1, failed: %2").arg(imported).arg(failed) << "\n";
    }
    return 0;
}

int CLIController::cmdList(const QStringList& args) {
    Q_UNUSED(args);
    auto* d = db();
    if (!d) return 2;

    auto r = d->listTracks(50);
    if (!r) {
        stdErr() << r.error().message << "\n";
        return 2;
    }

    if (m_format == "json") {
        QJsonArray arr;
        for (const auto& t : r.value()) {
            arr.append(QJsonObject{
                {"id", t.id},
                {"title", t.title},
                {"artist", t.artist},
                {"album", t.album},
                {"format", audioFormatToString(t.format)},
                {"duration_ms", t.durationMs},
                {"filepath", t.filepath},
            });
        }
        stdOut() << QJsonDocument(arr).toJson(QJsonDocument::Compact) << "\n";
    } else {
        stdOut() << QString("%1  %2  %3  %4  %5\n")
            .arg("ID", -5).arg("Title", -40).arg("Artist", -25)
            .arg("Album", -25).arg("Fmt", -5);
        for (const auto& t : r.value()) {
            stdOut() << QString("%1  %2  %3  %4  %5\n")
                .arg(t.id, -5)
                .arg(t.title.left(40), -40)
                .arg(t.artist.left(25), -25)
                .arg(t.album.left(25), -25)
                .arg(audioFormatToString(t.format), -5);
        }
    }
    return 0;
}

int CLIController::cmdSearch(const QStringList& args) {
    if (args.isEmpty()) {
        stdErr() << "Usage: search <query>\n";
        return 1;
    }
    auto* d = db();
    if (!d) return 2;

    auto r = d->searchTracks(args.join(' '), 50);
    if (!r) {
        stdErr() << r.error().message << "\n";
        return 2;
    }

    for (const auto& t : r.value()) {
        stdOut() << QString("[%1] %2 — %3\n").arg(t.id).arg(t.title, t.artist);
    }
    return 0;
}

int CLIController::cmdInfo(const QStringList& args) {
    if (args.isEmpty()) {
        stdErr() << "Usage: info <id>\n";
        return 1;
    }
    auto* d = db();
    if (!d) return 2;

    bool ok;
    const int id = args[0].toInt(&ok);
    if (!ok) { stdErr() << "Invalid ID\n"; return 1; }

    auto r = d->getTrack(id);
    if (!r) {
        stdErr() << r.error().message << "\n";
        return 2;
    }
    const auto& t = r.value();

    if (m_format == "json") {
        QJsonObject o{
            {"id", t.id}, {"title", t.title}, {"artist", t.artist},
            {"album", t.album}, {"genre", t.genre}, {"year", t.year},
            {"duration_ms", t.durationMs}, {"bitrate", t.bitrate},
            {"format", audioFormatToString(t.format)},
            {"play_count", t.playCount},
            {"filepath", t.filepath},
            {"acoustid", t.acoustid},
        };
        if (t.rgTrackGain.has_value()) o["rg_track_gain"] = *t.rgTrackGain;
        stdOut() << QJsonDocument(o).toJson() << "\n";
    } else {
        stdOut() << "ID:         " << t.id << "\n"
                 << "Title:      " << t.title << "\n"
                 << "Artist:     " << t.artist << "\n"
                 << "Album:      " << t.album << "\n"
                 << "Genre:      " << t.genre << "\n"
                 << "Year:       " << t.year << "\n"
                 << "Duration:   " << (t.durationMs / 1000) << "s\n"
                 << "Bitrate:    " << t.bitrate << " kbps\n"
                 << "Format:     " << audioFormatToString(t.format) << "\n"
                 << "Play count: " << t.playCount << "\n"
                 << "AcoustID:   " << (t.acoustid.isEmpty() ? QString("—") : t.acoustid) << "\n"
                 << "Path:       " << t.filepath << "\n";
    }
    return 0;
}

int CLIController::cmdPlay(const QStringList& args) {
    if (args.isEmpty()) {
        stdErr() << "Usage: play <query|id|path>\n";
        return 1;
    }

    if (tryDelegate(QStringList() << "play" << args)) return 0;

    auto* p = player();
    if (!p) return 2;

    const QString arg = args.join(' ');
    if (QFileInfo::exists(arg)) {
        auto r = p->playFile(arg);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << "Playing: " << arg << "\n";
        return 0;
    }

    bool ok;
    int id = arg.toInt(&ok);
    if (ok) {
        auto* d = db();
        if (!d) return 2;
        auto r = d->getTrack(id);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        p->play(r.value());
        stdOut() << "Playing [" << id << "]: " << r.value().title << "\n";
        return 0;
    }

    auto* d = db();
    if (!d) return 2;
    auto r = d->searchTracks(arg, 1);
    if (!r || r.value().isEmpty()) {
        stdErr() << "No match for: " << arg << "\n";
        return 2;
    }
    p->play(r.value().first());
    stdOut() << "Playing: " << r.value().first().title << "\n";
    return 0;
}

int CLIController::cmdPause()  { if (auto* p = player()) p->pause();  return 0; }
int CLIController::cmdResume() { if (auto* p = player()) p->resume(); return 0; }
int CLIController::cmdStop()   { if (auto* p = player()) p->stop();   return 0; }
int CLIController::cmdNext() {
    if (tryDelegate(QStringList() << "next")) return 0;
    stdErr() << QCoreApplication::translate("CLI",
        "next/prev need a running player (GUI or 'soundshelf serve') — "
        "the CLI has no persistent queue of its own.") << "\n";
    return 1;
}
int CLIController::cmdPrev() {
    if (tryDelegate(QStringList() << "prev")) return 0;
    stdErr() << QCoreApplication::translate("CLI",
        "next/prev need a running player (GUI or 'soundshelf serve') — "
        "the CLI has no persistent queue of its own.") << "\n";
    return 1;
}

int CLIController::cmdStatus() {
    auto* p = player();
    if (!p) return 2;
    stdOut() << "State:    " << static_cast<int>(p->state()) << "\n"
             << "Position: " << (p->positionMs() / 1000) << "s / "
                              << (p->durationMs() / 1000) << "s\n"
             << "Volume:   " << p->volumePercent() << "%\n";
    return 0;
}

int CLIController::cmdSeek(const QStringList& args) {
    if (args.isEmpty()) { stdErr() << "Usage: seek <pos>\n"; return 1; }
    auto* p = player();
    if (!p) return 2;
    const QString a = args[0];
    bool ok;
    if (a.startsWith('+') || a.startsWith('-')) {
        const int delta = a.toInt(&ok) * 1000;
        if (ok) p->seekRelative(delta);
    } else {
        const int abs = a.toInt(&ok) * 1000;
        if (ok) p->seekMs(abs);
    }
    return 0;
}

int CLIController::cmdVolume(const QStringList& args) {
    if (args.isEmpty()) { stdErr() << "Usage: volume <0-100|+5|-5>\n"; return 1; }
    auto* p = player();
    if (!p) return 2;
    const QString a = args[0];
    bool ok;
    if (a.startsWith('+') || a.startsWith('-')) {
        const int delta = a.toInt(&ok);
        if (ok) p->setVolume(p->volumePercent() + delta);
    } else {
        const int v = a.toInt(&ok);
        if (ok) p->setVolume(v);
    }
    return 0;
}

int CLIController::cmdTag(const QStringList& args) {
    if (args.size() < 2 || args[0] != "show") {
        stdErr() << "Usage: tag show <path>\n";
        return 1;
    }
    auto r = TagInfo::fromFile(args[1]);
    if (!r) { stdErr() << r.error().message << "\n"; return 2; }
    const auto& t = r.value();
    stdOut() << "Title:       " << t.title << "\n"
             << "Artist:      " << t.artist << "\n"
             << "Album:       " << t.album << "\n"
             << "Year:        " << t.year << "\n"
             << "Genre:       " << t.genre << "\n"
             << "Track:       " << t.trackNumber << "/" << t.trackTotal << "\n"
             << "Disc:        " << t.discNumber << "/" << t.discTotal << "\n"
             << "Duration:    " << (t.durationMs / 1000) << "s\n"
             << "Bitrate:     " << t.bitrate << "\n";
    if (t.rgTrackGain.has_value())
        stdOut() << "RG track:    " << *t.rgTrackGain << " dB\n";
    if (!t.acoustid.isEmpty())
        stdOut() << "AcoustID:    " << t.acoustid << "\n";
    return 0;
}

int CLIController::cmdDisc(const QStringList& args) {
    if (args.isEmpty()) {
        stdErr() << "Usage: disc list|search|add|rip|tracks|play|lookup\n";
        return 1;
    }
    auto* d = db();
    if (!d) return 2;
    const QString sub = args[0];

    if (sub == "list") {
        auto r = d->listDiscs(DiscType::Folder, 100);
        if (!r) return 2;
        for (const auto& dd : r.value()) {
            stdOut() << QString("[%1] %2 — %3 (%4)\n")
                .arg(dd.id).arg(dd.title, dd.artist).arg(discTypeToString(dd.type));
        }
        return 0;
    }
    if (sub == "search") {
        if (args.size() < 2) { stdErr() << "Usage: disc search <query>\n"; return 1; }
        auto r = d->searchDiscs(args.mid(1).join(' '));
        if (!r) return 2;
        for (const auto& dd : r.value()) {
            stdOut() << QString("[%1] %2 — %3\n").arg(dd.id).arg(dd.title, dd.artist);
        }
        return 0;
    }
    if (sub == "add") {
        const int fi = args.indexOf(QStringLiteral("--folder"));
        const int di = args.indexOf(QStringLiteral("--drive"));
        DiscManager dm;
        if (fi >= 0 && fi + 1 < args.size()) {
            auto r = dm.addFromFolder(args[fi + 1]);
            if (!r) { stdErr() << r.error().message << "\n"; return 2; }
            stdOut() << QCoreApplication::translate("CLI",
                "Added disc [%1] from folder").arg(r.value()) << "\n";
            return 0;
        }
        if (di >= 0 && di + 1 < args.size()) {
            auto r = dm.addFromCdda(args[di + 1]);
            if (!r) { stdErr() << r.error().message << "\n"; return 2; }
            stdOut() << QCoreApplication::translate("CLI",
                "Added disc [%1] from drive").arg(r.value()) << "\n";
            return 0;
        }
        stdErr() << "Usage: disc add --folder <path> | --drive <device>\n";
        return 1;
    }
    if (sub == "tracks") {
        if (args.size() < 2) { stdErr() << "Usage: disc tracks <id>\n"; return 1; }
        const int id = args[1].toInt();
        auto r = d->tracksByDisc(id);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        for (const auto& t : r.value()) {
            stdOut() << QString("%1. %2 — %3 (%4s)\n")
                .arg(t.trackNumber, 2).arg(t.title, t.artist)
                .arg(t.durationMs / 1000);
        }
        return 0;
    }
    if (sub == "play") {
        if (args.size() < 2) { stdErr() << "Usage: disc play <id>\n"; return 1; }
        auto r = d->tracksByDisc(args[1].toInt());
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        if (r.value().isEmpty()) { stdErr() << "Disc has no tracks\n"; return 2; }
        auto* p = player();
        if (!p) return 2;
        p->play(r.value().first());
        stdOut() << QCoreApplication::translate("CLI", "Playing disc: %1 track(s)")
                    .arg(r.value().size()) << "\n";
        return 0;
    }
    if (sub == "rip" || sub == "lookup") {
        stdErr() << QCoreApplication::translate("CLI",
            "disc %1 needs an optical drive and is best run interactively in the GUI.")
            .arg(sub) << "\n";
        return 1;
    }
    stdErr() << QCoreApplication::translate("CLI", "Unknown disc subcommand: %1").arg(sub) << "\n";
    return 1;
}

int CLIController::cmdReplaygain(const QStringList& args) {
    if (!ReplayGainAnalyzer::isAvailable()) {
        stdErr() << QCoreApplication::translate("CLI",
            "ReplayGain analysis needs libebur128 (not compiled in).") << "\n";
        return 1;
    }
    if (args.isEmpty()) {
        stdErr() << "Usage: replaygain <id|path> [--write] | --all [--write]\n";
        return 1;
    }
    const bool write = args.contains(QStringLiteral("--write"));
    ReplayGainAnalyzer rg;

    // Build the work list: (trackId or -1, filepath).
    QList<QPair<int, QString>> work;
    if (args.contains(QStringLiteral("--all"))) {
        auto* d = db(); if (!d) return 2;
        auto r = d->listTracks(1000000);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        for (const auto& t : r.value()) work.append({t.id, t.filepath});
    } else {
        QString target = args[0];
        bool isId; const int id = target.toInt(&isId);
        if (isId) {
            auto* d = db(); if (!d) return 2;
            auto r = d->getTrack(id);
            if (!r) { stdErr() << r.error().message << "\n"; return 2; }
            work.append({id, r.value().filepath});
        } else {
            work.append({-1, target});
        }
    }

    int done = 0, failed = 0;
    for (const auto& [trackId, path] : work) {
        auto r = rg.analyseFile(path);
        if (!r) { ++failed; stdErr() << "x " << path << ": " << r.error().message << "\n"; continue; }
        const auto& tr = r.value();
        if (!m_quiet)
            stdOut() << QString("%1  %2 LUFS  gain %3 dB  peak %4\n")
                .arg(QFileInfo(path).fileName().left(40), -40)
                .arg(tr.integratedLufs, 6, 'f', 1)
                .arg(tr.trackGainDb, 6, 'f', 2)
                .arg(tr.trackPeak, 0, 'f', 4);
        if (trackId >= 0) {
            if (auto* d = db())
                d->updateReplayGain(trackId, tr.trackGainDb, tr.trackPeak);
        }
        if (write) {
            auto w = rg.writeTagsTrack(path, tr);
            if (!w) stdErr() << "  (tag write failed: " << w.error().message << ")\n";
        }
        ++done;
    }
    stdOut() << QCoreApplication::translate("CLI", "ReplayGain: %1 analysed, %2 failed")
                .arg(done).arg(failed) << "\n";
    return failed && !done ? 2 : 0;
}

int CLIController::cmdFingerprint(const QStringList& args) {
    if (!ChromaprintEngine::isAvailable()) {
        stdErr() << QCoreApplication::translate("CLI",
            "Fingerprinting needs libchromaprint (not compiled in).") << "\n";
        return 1;
    }
    if (args.isEmpty()) {
        stdErr() << "Usage: fingerprint <id|path> [--lookup] | --all [--lookup]\n";
        return 1;
    }
    const bool lookup = args.contains(QStringLiteral("--lookup"));

    QList<QPair<int, QString>> work;
    if (args.contains(QStringLiteral("--all"))) {
        auto* d = db(); if (!d) return 2;
        auto r = d->listTracks(1000000);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        for (const auto& t : r.value()) work.append({t.id, t.filepath});
    } else {
        QString target = args[0];
        bool isId; const int id = target.toInt(&isId);
        if (isId) {
            auto* d = db(); if (!d) return 2;
            auto r = d->getTrack(id);
            if (!r) { stdErr() << r.error().message << "\n"; return 2; }
            work.append({id, r.value().filepath});
        } else {
            work.append({-1, target});
        }
    }

    // AcoustID lookup needs a per-application API key (acoustid.api_key).
    QString apiKey;
    if (lookup) {
        if (auto* d = db()) {
            if (auto k = d->getSetting(QStringLiteral("acoustid.api_key")); k)
                apiKey = k.value();
        }
        if (apiKey.isEmpty()) {
            stdErr() << QCoreApplication::translate("CLI",
                "--lookup needs an AcoustID API key. Set it with: "
                "soundshelf db ... or store 'acoustid.api_key' in settings "
                "(get a free key at https://acoustid.org/).") << "\n";
            return 1;
        }
    }

    ChromaprintEngine cp;
    int done = 0, failed = 0;
    for (const auto& [trackId, path] : work) {
        auto r = cp.fingerprintFile(path);
        if (!r) { ++failed; stdErr() << "x " << path << ": " << r.error().message << "\n"; continue; }
        const auto& fp = r.value();
        if (!m_quiet)
            stdOut() << QString("%1  %2s  fp[%3]\n")
                .arg(QFileInfo(path).fileName().left(36), -36)
                .arg(fp.durationSec).arg(fp.fingerprint.size());
        if (m_verbose) stdOut() << fp.fingerprint << "\n";

        if (lookup) {
            AcoustIDClient client;
            client.setApiKey(apiKey);
            QFutureWatcher<Result<QJsonDocument>> watcher;
            QEventLoop loop;
            QObject::connect(&watcher, &QFutureWatcherBase::finished, &loop, &QEventLoop::quit);
            watcher.setFuture(client.lookup(fp.fingerprint, fp.durationSec));
            loop.exec();
            const auto res = watcher.result();
            if (!res) { stdErr() << "  (lookup failed: " << res.error().message << ")\n"; }
            else {
                const auto results = res.value().object().value("results").toArray();
                QString mbid;
                if (!results.isEmpty()) {
                    const auto recs = results.first().toObject().value("recordings").toArray();
                    if (!recs.isEmpty()) mbid = recs.first().toObject().value("id").toString();
                }
                if (!mbid.isEmpty()) {
                    stdOut() << "  → MBID " << mbid << "\n";
                    if (trackId >= 0) {
                        if (auto* d = db()) {
                            QSqlQuery q(d->database());
                            q.prepare(QStringLiteral(
                                "UPDATE tracks SET acoustid = ?, mb_recording_id = ? WHERE id = ?"));
                            q.addBindValue(fp.fingerprint);
                            q.addBindValue(mbid);
                            q.addBindValue(trackId);
                            q.exec();
                        }
                    }
                } else {
                    stdOut() << "  → no AcoustID match\n";
                }
            }
        }
        ++done;
    }
    stdOut() << QCoreApplication::translate("CLI", "Fingerprint: %1 done, %2 failed")
                .arg(done).arg(failed) << "\n";
    return failed && !done ? 2 : 0;
}

int CLIController::cmdConvert(const QStringList& args) {
    const int toi = args.indexOf(QStringLiteral("--to"));
    if (args.isEmpty() || toi < 0 || toi + 1 >= args.size()) {
        stdErr() << "Usage: convert <id|path> --to flac|mp3|ogg|opus|aac|wav\n";
        return 1;
    }
    // Resolve input to a filesystem path (id or path).
    QString input = args[0];
    bool isId; const int id = input.toInt(&isId);
    if (isId) {
        auto* d = db(); if (!d) return 2;
        auto r = d->getTrack(id);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        input = r.value().filepath;
    }
    if (!QFileInfo::exists(input)) {
        stdErr() << QCoreApplication::translate("CLI", "Input not found: %1").arg(input) << "\n";
        return 1;
    }

    using F = FormatConverter::Format;
    static const QHash<QString, F> map{
        {QStringLiteral("mp3"),  F::Mp3V0},   {QStringLiteral("flac"), F::Flac},
        {QStringLiteral("ogg"),  F::OggVorbis},{QStringLiteral("opus"), F::Opus_128},
        {QStringLiteral("aac"),  F::Aac_256}, {QStringLiteral("wav"),  F::WavPcm16},
    };
    static const QHash<QString, QString> ext{
        {QStringLiteral("mp3"),"mp3"},{QStringLiteral("flac"),"flac"},
        {QStringLiteral("ogg"),"ogg"},{QStringLiteral("opus"),"opus"},
        {QStringLiteral("aac"),"m4a"},{QStringLiteral("wav"),"wav"},
    };
    const QString to = args[toi + 1].toLower();
    if (!map.contains(to)) { stdErr() << "Unknown target format: " << to << "\n"; return 1; }

    FormatConverter::Job job;
    job.input  = input;
    job.format = map.value(to);
    job.overwrite = args.contains(QStringLiteral("--overwrite"));
    QFileInfo fi(input);
    job.output = fi.dir().filePath(fi.completeBaseName() + "." + ext.value(to));

    FormatConverter conv;
    bool ok = false; QString msg;
    QEventLoop loop;
    QObject::connect(&conv, &FormatConverter::progress, &conv, [this](int pct) {
        if (!m_quiet) stdErr() << "\r" << pct << "%   " << Qt::flush;
    });
    QObject::connect(&conv, &FormatConverter::finished, &loop,
        [&](bool good, const QString& m) { ok = good; msg = m; loop.quit(); });
    auto started = conv.start(job);
    if (!started) { stdErr() << started.error().message << "\n"; return 2; }
    loop.exec();
    if (!m_quiet) stdErr() << "\n";
    if (!ok) { stdErr() << "Conversion failed: " << msg << "\n"; return 2; }
    stdOut() << QCoreApplication::translate("CLI", "Converted → %1").arg(job.output) << "\n";
    return 0;
}

int CLIController::cmdDuplicates(const QStringList& args) {
    const QString sub = args.value(0, QStringLiteral("scan"));
    if (sub != "scan") {
        stdErr() << QCoreApplication::translate("CLI",
            "Usage: duplicates scan   (resolution is interactive — use the GUI)") << "\n";
        return 1;
    }
    if (!db()) return 2;
    DuplicateDetector det;
    auto r = det.findDuplicates();
    if (!r) { stdErr() << r.error().message << "\n"; return 2; }
    const auto groups = r.value();
    if (groups.isEmpty()) {
        stdOut() << QCoreApplication::translate("CLI", "No duplicates found.") << "\n";
        return 0;
    }
    int n = 0;
    for (const auto& g : groups) {
        stdOut() << QCoreApplication::translate("CLI", "Group %1 (%2 tracks):")
                    .arg(++n).arg(g.tracks.size()) << "\n";
        for (const auto& t : g.tracks)
            stdOut() << "  [" << t.id << "] " << t.filepath << "\n";
    }
    stdOut() << QCoreApplication::translate("CLI", "%1 duplicate group(s).").arg(groups.size()) << "\n";
    return 0;
}

int CLIController::cmdPlaylist(const QStringList& args) {
    const QString sub = args.value(0);
    if (!db()) return 2;
    PlaylistManager pm;

    if (sub.isEmpty() || sub == "list") {
        auto r = pm.list();
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        for (const auto& pl : r.value())
            stdOut() << QString("[%1] %2%3\n").arg(pl.id).arg(pl.name,
                        pl.smart ? QStringLiteral(" (smart)") : QString());
        return 0;
    }
    if (sub == "create") {
        if (args.size() < 2) { stdErr() << "Usage: playlist create <name>\n"; return 1; }
        auto r = pm.create(args.mid(1).join(' '));
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Created playlist [%1]").arg(r.value()) << "\n";
        return 0;
    }
    if (sub == "export") {
        if (args.size() < 3) { stdErr() << "Usage: playlist export <id> <file.m3u|.pls|.xspf>\n"; return 1; }
        auto tr = pm.tracksOf(args[1].toInt());
        if (!tr) { stdErr() << tr.error().message << "\n"; return 2; }
        PlaylistExporter exp;
        auto r = exp.exportToFile(args[2], tr.value());
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Exported %1 track(s) → %2")
                    .arg(tr.value().size()).arg(args[2]) << "\n";
        return 0;
    }
    if (sub == "import") {
        if (args.size() < 3) { stdErr() << "Usage: playlist import <file> <name>\n"; return 1; }
        PlaylistImporter imp;
        auto entries = imp.importFile(args[1]);
        if (!entries) { stdErr() << entries.error().message << "\n"; return 2; }
        auto* d = db();
        auto created = pm.create(args.mid(2).join(' '));
        if (!created) { stdErr() << created.error().message << "\n"; return 2; }
        int matched = 0;
        for (const auto& e : entries.value()) {
            auto t = d->getTrackByPath(e.path);
            if (t) { pm.appendTrack(created.value(), t.value().id); ++matched; }
        }
        stdOut() << QCoreApplication::translate("CLI",
            "Imported playlist [%1]: %2/%3 track(s) matched in library")
            .arg(created.value()).arg(matched).arg(entries.value().size()) << "\n";
        return 0;
    }
    stdErr() << "Usage: playlist list|create|export|import\n";
    return 1;
}

void CLIController::setPodcastFetcherForTesting(PodcastManager::FeedFetcher fetcher) {
    m_podcastFetcher = std::move(fetcher);
}

int CLIController::cmdPodcast(const QStringList& args) {
    const QString sub = args.value(0);

    if (sub.isEmpty()) {
        stdErr() << QCoreApplication::translate("CLI",
            "Usage: podcast list|subscribe|refresh|episodes|download|played|unsubscribe") << "\n";
        return 1;
    }

    if (sub == "list") {
        if (!db()) return 2;
        PodcastStore store;
        auto r = store.feeds();
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        if (m_format == QLatin1String("json")) {
            QJsonArray arr;
            for (const auto& f : r.value()) {
                arr.append(QJsonObject{
                    {QStringLiteral("id"),             f.id},
                    {QStringLiteral("url"),            f.url},
                    {QStringLiteral("title"),          f.title},
                    {QStringLiteral("author"),         f.author},
                    {QStringLiteral("language"),       f.language},
                    {QStringLiteral("last_refreshed"), f.lastRefreshed.toString(Qt::ISODate)},
                });
            }
            stdOut() << QJsonDocument(arr).toJson(QJsonDocument::Compact) << "\n";
        } else {
            stdOut() << QString("%1  %2  %3\n").arg("ID", -4).arg("Title", -40).arg("Author", -25);
            for (const auto& f : r.value()) {
                stdOut() << QString("%1  %2  %3\n")
                    .arg(f.id, -4)
                    .arg(f.title.left(40), -40)
                    .arg(f.author.left(25), -25);
            }
        }
        return 0;
    }

    if (sub == "subscribe") {
        if (args.size() < 2) {
            stdErr() << QCoreApplication::translate("CLI", "Usage: podcast subscribe <url>") << "\n";
            return 1;
        }
        if (!db()) return 2;
        PodcastManager mgr;
        if (m_podcastFetcher) {
            mgr.setFeedFetcher(m_podcastFetcher);
            mgr.setEnclosureFetcher(m_podcastFetcher);
        }
        auto r = mgr.subscribe(args[1]);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Subscribed feed [%1]").arg(r.value()) << "\n";
        return 0;
    }

    if (sub == "refresh") {
        if (!db()) return 2;
        PodcastManager mgr;
        if (m_podcastFetcher) {
            mgr.setFeedFetcher(m_podcastFetcher);
            mgr.setEnclosureFetcher(m_podcastFetcher);
        }
        if (args.contains(QStringLiteral("--all"))) {
            auto r = mgr.refreshAll();
            if (!r) { stdErr() << r.error().message << "\n"; return 2; }
            stdOut() << QCoreApplication::translate("CLI",
                "Refreshed all feeds: %1 new episode(s)").arg(r.value()) << "\n";
            return 0;
        }
        if (args.size() < 2) {
            stdErr() << QCoreApplication::translate("CLI", "Usage: podcast refresh <id> | --all") << "\n";
            return 1;
        }
        bool ok;
        const int feedId = args[1].toInt(&ok);
        if (!ok) {
            stdErr() << QCoreApplication::translate("CLI", "Invalid feed id: %1").arg(args[1]) << "\n";
            return 1;
        }
        auto r = mgr.refreshFeed(feedId);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI",
            "Feed [%1] refreshed: %2 new episode(s)").arg(feedId).arg(r.value()) << "\n";
        return 0;
    }

    if (sub == "episodes") {
        if (args.size() < 2) {
            stdErr() << QCoreApplication::translate("CLI", "Usage: podcast episodes <feedId>") << "\n";
            return 1;
        }
        if (!db()) return 2;
        bool ok;
        const int feedId = args[1].toInt(&ok);
        if (!ok) {
            stdErr() << QCoreApplication::translate("CLI", "Invalid feed id: %1").arg(args[1]) << "\n";
            return 1;
        }
        PodcastStore store;
        auto r = store.episodesForFeed(feedId);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        if (m_format == QLatin1String("json")) {
            QJsonArray arr;
            for (const auto& ep : r.value()) {
                QJsonObject o{
                    {QStringLiteral("id"),             ep.id},
                    {QStringLiteral("feed_id"),        ep.feedId},
                    {QStringLiteral("title"),          ep.title},
                    {QStringLiteral("episode_number"), ep.episodeNumber},
                    {QStringLiteral("duration_s"),     ep.durationMs / 1000},
                    {QStringLiteral("played"),         ep.isPlayed},
                };
                if (!ep.localPath.isEmpty())
                    o[QStringLiteral("local_path")] = ep.localPath;
                arr.append(o);
            }
            stdOut() << QJsonDocument(arr).toJson(QJsonDocument::Compact) << "\n";
        } else {
            stdOut() << QString("%1  %2  %3  %4  %5\n")
                .arg("#",  -4).arg("Title", -45).arg("Dur(s)", -7).arg("Pl", -3).arg("Local");
            for (const auto& ep : r.value()) {
                stdOut() << QString("%1  %2  %3  %4  %5\n")
                    .arg(ep.episodeNumber, -4)
                    .arg(ep.title.left(45), -45)
                    .arg(ep.durationMs / 1000, -7)
                    .arg(ep.isPlayed ? 'Y' : 'N')
                    .arg(ep.localPath.isEmpty() ? QStringLiteral("-") : ep.localPath);
            }
        }
        return 0;
    }

    if (sub == "download") {
        if (args.size() < 2) {
            stdErr() << QCoreApplication::translate("CLI", "Usage: podcast download <episodeId> [--dir PATH]") << "\n";
            return 1;
        }
        if (!db()) return 2;
        bool ok;
        const int episodeId = args[1].toInt(&ok);
        if (!ok) {
            stdErr() << QCoreApplication::translate("CLI", "Invalid episode id: %1").arg(args[1]) << "\n";
            return 1;
        }
        const int di = args.indexOf(QStringLiteral("--dir"));
        const QString dir = (di >= 0 && di + 1 < args.size())
            ? args[di + 1]
            : QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
              + QStringLiteral("/Podcasts");
        QDir().mkpath(dir);
        PodcastManager mgr;
        if (m_podcastFetcher) {
            mgr.setFeedFetcher(m_podcastFetcher);
            mgr.setEnclosureFetcher(m_podcastFetcher);
        }
        auto r = mgr.downloadEpisode(episodeId, dir);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Downloaded → %1").arg(r.value()) << "\n";
        return 0;
    }

    if (sub == "played") {
        if (args.size() < 2) {
            stdErr() << QCoreApplication::translate("CLI", "Usage: podcast played <episodeId> [--unset]") << "\n";
            return 1;
        }
        if (!db()) return 2;
        bool ok;
        const int episodeId = args[1].toInt(&ok);
        if (!ok) {
            stdErr() << QCoreApplication::translate("CLI", "Invalid episode id: %1").arg(args[1]) << "\n";
            return 1;
        }
        const bool unset = args.contains(QStringLiteral("--unset"));
        PodcastStore store;
        auto r = store.setPlayed(episodeId, !unset);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Episode [%1] marked as %2")
            .arg(episodeId)
            .arg(unset
                ? QCoreApplication::translate("CLI", "unplayed")
                : QCoreApplication::translate("CLI", "played")) << "\n";
        return 0;
    }

    if (sub == "unsubscribe") {
        if (args.size() < 2) {
            stdErr() << QCoreApplication::translate("CLI", "Usage: podcast unsubscribe <feedId>") << "\n";
            return 1;
        }
        if (!db()) return 2;
        bool ok;
        const int feedId = args[1].toInt(&ok);
        if (!ok) {
            stdErr() << QCoreApplication::translate("CLI", "Invalid feed id: %1").arg(args[1]) << "\n";
            return 1;
        }
        PodcastStore store;
        auto r = store.unsubscribe(feedId);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Unsubscribed feed [%1]").arg(feedId) << "\n";
        return 0;
    }

    stdErr() << QCoreApplication::translate("CLI",
        "Unknown podcast subcommand: %1. "
        "Use list|subscribe|refresh|episodes|download|played|unsubscribe.").arg(sub) << "\n";
    return 1;
}

int CLIController::cmdRemote(const QStringList& args) {
    Q_UNUSED(args);
    stdErr() << QCoreApplication::translate("CLI",
        "remote talks to a SoundShelf server over HTTP. Configure the server URL and "
        "bearer token first (see 'soundshelf serve'); remote control is exposed via the "
        "REST API and the GUI's remote-library panel.") << "\n";
    return 1;
}

int CLIController::cmdServe(const QStringList& args) {
    if (!HttpServer::isAvailable()) {
        stdErr() << QCoreApplication::translate("CLI",
            "Built without QHttpServer (needs Qt 6.4+ HttpServer module).") << "\n";
        return 1;
    }
    auto* d = db();
    if (!d) return 2;

    quint16 port = 8080;
    QHostAddress bind = QHostAddress::AnyIPv4;
    QString token;
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size())  port = args[++i].toUShort();
        else if (args[i] == "--bind" && i + 1 < args.size()) bind = QHostAddress(args[++i]);
        else if (args[i] == "--auth" && i + 1 < args.size()) token = args[++i];
    }
    if (token.isEmpty()) {
        auto stored = d->getSetting(QStringLiteral("server.bearer_token"));
        token = (stored && !stored.value().isEmpty())
            ? stored.value()
            : QUuid::createUuid().toString(QUuid::WithoutBraces);
        d->setSetting(QStringLiteral("server.bearer_token"), token);
    }

    HttpServer server;
    server.setBearerToken(token);
    auto r = server.start(bind, port);
    if (!r) { stdErr() << "Cannot start HTTP server: " << r.error().message << "\n"; return 2; }
    stdOut() << QCoreApplication::translate("CLI", "Serving on %1:%2 — bearer token: %3")
                .arg(bind.toString()).arg(port).arg(token) << "\n";
    QEventLoop loop;   // run until terminated
    loop.exec();
    return 0;
}

int CLIController::cmdDaemon(const QStringList& args) {
    const QString sub = args.value(0);
    if (sub == "start")
        stdErr() << QCoreApplication::translate("CLI",
            "Run 'soundshelf serve' (optionally under systemd/Task Scheduler) to daemonise.") << "\n";
    else
        stdErr() << QCoreApplication::translate("CLI",
            "daemon start|stop|status — process supervision is delegated to the OS "
            "(systemd on Linux, Task Scheduler on Windows).") << "\n";
    return 1;
}

int CLIController::cmdScrobble(const QStringList& args) {
    const QString sub = args.value(0, QStringLiteral("status"));
    if (!db()) return 2;
    Scrobbler sc;
    if (sub == "status") {
        auto r = sc.pendingRows(1000);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Pending scrobbles: %1").arg(r.value().size()) << "\n";
        for (const auto& row : r.value())
            stdOut() << "  #" << row.id << " track=" << row.trackId << " via " << row.service << "\n";
        return 0;
    }
    if (sub == "flush") {
        stdErr() << QCoreApplication::translate("CLI",
            "Flushing requires authenticated Last.fm/ListenBrainz sessions and runs from the "
            "GUI/daemon drainer. Use 'scrobble auth' there first.") << "\n";
        return 1;
    }
    if (sub == "auth") {
        stdErr() << QCoreApplication::translate("CLI",
            "Authentication uses a browser-based OAuth flow — run it from the GUI.") << "\n";
        return 1;
    }
    stdErr() << "Usage: scrobble status|flush|auth\n";
    return 1;
}

int CLIController::cmdPlugin(const QStringList& args) {
    const QString sub = args.value(0, QStringLiteral("list"));
    if (sub != "list") {
        stdErr() << QCoreApplication::translate("CLI",
            "Usage: plugin list   (install/enable/disable are managed in the GUI)") << "\n";
        return 1;
    }
    PluginManager pm;
    pm.scan();
    const auto plugins = pm.plugins();
    if (plugins.isEmpty()) {
        stdOut() << QCoreApplication::translate("CLI", "No visualization plugins found.") << "\n";
        stdOut() << QCoreApplication::translate("CLI", "Search paths:") << "\n";
        for (const auto& p : PluginManager::defaultPluginPaths())
            stdOut() << "  " << p << "\n";
        return 0;
    }
    stdOut() << QCoreApplication::translate("CLI", "%1 plugin(s) found.").arg(plugins.size()) << "\n";
    return 0;
}

int CLIController::cmdStats(const QStringList& args) {
    const QString sub = args.value(0, QStringLiteral("top-tracks"));
    if (!db()) return 2;
    PlayHistory hist;
    if (sub == "top-tracks") {
        auto r = hist.topTracks(25);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        for (const auto& a : r.value())
            stdOut() << QString("%1×  %2\n").arg(a.playCount, 4).arg(a.label);
        return 0;
    }
    if (sub == "listening-time") {
        auto r = hist.totalPlayedMs();
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        const qint64 sec = r.value() / 1000;
        stdOut() << QCoreApplication::translate("CLI", "Total listening time: %1h %2m")
                    .arg(sec / 3600).arg((sec % 3600) / 60) << "\n";
        return 0;
    }
    if (sub == "heatmap") {
        auto r = hist.playsPerWeekday(365);
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        static const char* days[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
        const auto v = r.value();
        for (int i = 0; i < v.size() && i < 7; ++i)
            stdOut() << days[i] << "  " << QString(v[i], QChar('#')) << " " << v[i] << "\n";
        return 0;
    }
    stdErr() << "Usage: stats top-tracks|listening-time|heatmap\n";
    return 1;
}

int CLIController::cmdExport(const QStringList& args) {
    const int oi = args.indexOf(QStringLiteral("--out"));
    // Output format comes from the global --format flag (json|table|csv);
    // "table" has no file meaning here, so it falls back to json.
    const QString fmt = (m_format == QLatin1String("csv")) ? QStringLiteral("csv")
                                                           : QStringLiteral("json");
    auto* d = db();
    if (!d) return 2;
    auto r = d->listTracks(1000000);
    if (!r) { stdErr() << r.error().message << "\n"; return 2; }
    const auto tracks = r.value();

    QString out;
    if (fmt == "json") {
        QJsonArray arr;
        for (const auto& t : tracks)
            arr.append(QJsonObject{
                {"id", t.id}, {"title", t.title}, {"artist", t.artist},
                {"album", t.album}, {"genre", t.genre}, {"year", t.year},
                {"duration_ms", t.durationMs}, {"filepath", t.filepath}});
        out = QString::fromUtf8(QJsonDocument(arr).toJson());
    } else if (fmt == "csv") {
        out = QStringLiteral("id,title,artist,album,genre,year,duration_ms,filepath\n");
        auto esc = [](QString s){ s.replace('"', QStringLiteral("\"\"")); return '"' + s + '"'; };
        for (const auto& t : tracks)
            out += QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
                .arg(t.id).arg(esc(t.title), esc(t.artist), esc(t.album), esc(t.genre))
                .arg(t.year).arg(t.durationMs).arg(esc(t.filepath));
    } else {
        stdErr() << "Unknown export format: " << fmt << " (use json|csv)\n";
        return 1;
    }

    if (oi >= 0 && oi + 1 < args.size()) {
        QFile f(args[oi + 1]);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            stdErr() << "Cannot write: " << args[oi + 1] << "\n"; return 2;
        }
        f.write(out.toUtf8());
        stdOut() << QCoreApplication::translate("CLI", "Exported %1 track(s) → %2")
                    .arg(tracks.size()).arg(args[oi + 1]) << "\n";
    } else {
        stdOut() << out;
    }
    return 0;
}

int CLIController::cmdDb(const QStringList& args) {
    const QString sub = args.value(0);
    auto* d = db();
    if (!d) return 2;
    if (sub == "migrate") {
        auto db = d->database();
        SchemaMigrator mig(db);
        auto r = mig.migrate();
        if (!r) { stdErr() << r.error().message << "\n"; return 2; }
        stdOut() << QCoreApplication::translate("CLI", "Schema at version %1").arg(mig.currentVersion()) << "\n";
        return 0;
    }
    if (sub == "vacuum") {
        QSqlQuery q(d->database());
        if (!q.exec(QStringLiteral("VACUUM"))) {
            stdErr() << q.lastError().text() << "\n"; return 2;
        }
        stdOut() << QCoreApplication::translate("CLI", "Database vacuumed.") << "\n";
        return 0;
    }
    if (sub == "info") {
        auto db = d->database();
        SchemaMigrator mig(db);
        auto tracks = d->listTracks(1000000);
        stdOut() << QCoreApplication::translate("CLI", "Schema version: %1").arg(mig.currentVersion()) << "\n";
        stdOut() << QCoreApplication::translate("CLI", "Tracks: %1")
                    .arg(tracks ? tracks.value().size() : 0) << "\n";
        stdOut() << QCoreApplication::translate("CLI", "Path: %1").arg(d->database().databaseName()) << "\n";
        return 0;
    }
    stdErr() << "Usage: db migrate|vacuum|info  (backup/restore: copy the .db file)\n";
    return 1;
}

bool CLIController::tryDelegate(const QStringList& args) {
    Q_UNUSED(args);
    // TODO: D-Bus / named pipe IPC do działającego GUI.
    // Zwracamy false — fallback na lokalną instancję PlayerEngine.
    return false;
}

} // namespace soundshelf
