#include "soundshelf/cli/CLIController.hpp"

#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/core/Translator.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/io/TagInfo.hpp"

#include <QCoreApplication>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
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
int CLIController::cmdNext()   { stdErr() << "TODO: next\n"; return 0; }
int CLIController::cmdPrev()   { stdErr() << "TODO: prev\n"; return 0; }

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
    stdErr() << "TODO: disc " << sub << "\n";
    return 0;
}

// Stubs — to be filled by Claude Code
int CLIController::cmdReplaygain(const QStringList& args)  { Q_UNUSED(args); stdErr() << "TODO: replaygain\n"; return 0; }
int CLIController::cmdFingerprint(const QStringList& args) { Q_UNUSED(args); stdErr() << "TODO: fingerprint\n"; return 0; }
int CLIController::cmdConvert(const QStringList& args)     { Q_UNUSED(args); stdErr() << "TODO: convert\n"; return 0; }
int CLIController::cmdDuplicates(const QStringList& args)  { Q_UNUSED(args); stdErr() << "TODO: duplicates\n"; return 0; }
int CLIController::cmdPlaylist(const QStringList& args)    { Q_UNUSED(args); stdErr() << "TODO: playlist\n"; return 0; }
int CLIController::cmdRemote(const QStringList& args)      { Q_UNUSED(args); stdErr() << "TODO: remote\n"; return 0; }
int CLIController::cmdServe(const QStringList& args)       { Q_UNUSED(args); stdErr() << "TODO: serve\n"; return 0; }
int CLIController::cmdDaemon(const QStringList& args)      { Q_UNUSED(args); stdErr() << "TODO: daemon\n"; return 0; }
int CLIController::cmdScrobble(const QStringList& args)    { Q_UNUSED(args); stdErr() << "TODO: scrobble\n"; return 0; }
int CLIController::cmdPlugin(const QStringList& args)      { Q_UNUSED(args); stdErr() << "TODO: plugin\n"; return 0; }
int CLIController::cmdStats(const QStringList& args)       { Q_UNUSED(args); stdErr() << "TODO: stats\n"; return 0; }
int CLIController::cmdExport(const QStringList& args)      { Q_UNUSED(args); stdErr() << "TODO: export\n"; return 0; }
int CLIController::cmdDb(const QStringList& args)          { Q_UNUSED(args); stdErr() << "TODO: db\n"; return 0; }

bool CLIController::tryDelegate(const QStringList& args) {
    Q_UNUSED(args);
    // TODO: D-Bus / named pipe IPC do działającego GUI.
    // Zwracamy false — fallback na lokalną instancję PlayerEngine.
    return false;
}

} // namespace soundshelf
