#include "soundshelf/io/CueParser.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCue, "soundshelf.io.cue")

namespace soundshelf {
namespace {

QString stripQuotes(QString s) {
    s = s.trimmed();
    if (s.size() >= 2 && s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"'))) {
        return s.mid(1, s.size() - 2);
    }
    return s;
}

QStringList tokenize(const QString& line) {
    QStringList out;
    QString cur;
    bool inQuotes = false;
    for (QChar ch : line) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            cur.append(ch);
        } else if (ch.isSpace() && !inQuotes) {
            if (!cur.isEmpty()) { out << cur; cur.clear(); }
        } else {
            cur.append(ch);
        }
    }
    if (!cur.isEmpty()) out << cur;
    return out;
}

/// Parse "MM:SS:FF" to total frames. CD-DA uses 75 frames per second.
long parseMsf(const QString& s) {
    const auto parts = s.split(QLatin1Char(':'));
    if (parts.size() != 3) return -1;
    bool ok1, ok2, ok3;
    long m   = parts[0].toLong(&ok1);
    long sec = parts[1].toLong(&ok2);
    long f   = parts[2].toLong(&ok3);
    if (!ok1 || !ok2 || !ok3) return -1;
    return ((m * 60) + sec) * 75 + f;
}

std::optional<double> tryDouble(const QString& s) {
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok) return std::nullopt;
    return v;
}

} // anonymous namespace

CueParser::CueParser(QObject* parent) : QObject(parent) {}
CueParser::~CueParser() = default;

int CueParser::framesToMs(long frames) {
    if (frames < 0) return 0;
    // 75 frames per second on CD-DA. (frames * 1000 + 37) / 75 rounds.
    return static_cast<int>((frames * 1000 + 37) / 75);
}

Result<CueParser::CueSheet> CueParser::parseFile(const QString& cuePath) {
    QFile f(cuePath);
    if (!f.exists()) {
        return Result<CueSheet>::err(Error::FileNotFound,
            QStringLiteral("CUE not found: %1").arg(cuePath));
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return Result<CueSheet>::err(Error::FileAccessDenied,
            QStringLiteral("Cannot open CUE: %1").arg(cuePath));
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    return parseString(ts.readAll(), QFileInfo(cuePath).fileName());
}

Result<CueParser::CueSheet> CueParser::parseString(const QString& text, const QString& label) {
    CueSheet sheet;
    CueTrack* current = nullptr;
    bool inTrackContext = false;

    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        const QStringList tok = tokenize(line);
        if (tok.isEmpty()) continue;
        const QString cmd = tok[0].toUpper();

        if (cmd == QLatin1String("FILE")) {
            if (tok.size() >= 2) sheet.file = stripQuotes(tok[1]);
            if (tok.size() >= 3) sheet.fileType = tok[2].toUpper();
        } else if (cmd == QLatin1String("TITLE")) {
            const QString val = tok.size() >= 2
                ? stripQuotes(tok.mid(1).join(QLatin1Char(' ')))
                : QString();
            if (inTrackContext && current) current->title = val;
            else sheet.albumTitle = val;
        } else if (cmd == QLatin1String("PERFORMER")) {
            const QString val = tok.size() >= 2
                ? stripQuotes(tok.mid(1).join(QLatin1Char(' ')))
                : QString();
            if (inTrackContext && current) current->performer = val;
            else sheet.albumPerformer = val;
        } else if (cmd == QLatin1String("ISRC")) {
            if (current && tok.size() >= 2) current->isrc = stripQuotes(tok[1]);
        } else if (cmd == QLatin1String("TRACK")) {
            if (tok.size() < 3) continue;
            CueTrack t;
            t.trackNumber = tok[1].toInt();
            sheet.tracks.append(t);
            current = &sheet.tracks.last();
            inTrackContext = true;
        } else if (cmd == QLatin1String("INDEX")) {
            if (!current || tok.size() < 3) continue;
            const int idx = tok[1].toInt();
            const long frames = parseMsf(tok[2]);
            if (idx == 0)      current->indexZeroFrames = frames;
            else if (idx == 1) current->indexOneFrames  = frames;
        } else if (cmd == QLatin1String("REM")) {
            if (tok.size() < 2) continue;
            const QString sub = tok[1].toUpper();
            if (sub == QLatin1String("DATE") || sub == QLatin1String("YEAR")) {
                if (tok.size() >= 3) sheet.year = tok[2].left(4).toInt();
            } else if (sub == QLatin1String("GENRE")) {
                sheet.albumGenre = stripQuotes(tok.mid(2).join(QLatin1Char(' ')));
            } else if (sub == QLatin1String("REPLAYGAIN_TRACK_GAIN") && current) {
                if (tok.size() >= 3) current->rgTrackGain = tryDouble(tok[2]);
            } else if (sub == QLatin1String("REPLAYGAIN_TRACK_PEAK") && current) {
                if (tok.size() >= 3) current->rgTrackPeak = tryDouble(tok[2]);
            } else if (sub == QLatin1String("REPLAYGAIN_ALBUM_GAIN")) {
                if (tok.size() >= 3) sheet.rgAlbumGain = tryDouble(tok[2]);
            } else if (sub == QLatin1String("REPLAYGAIN_ALBUM_PEAK")) {
                if (tok.size() >= 3) sheet.rgAlbumPeak = tryDouble(tok[2]);
            }
        }
        // FLAGS, PREGAP, POSTGAP, CDTEXTFILE, CATALOG — silently ignored
    }

    if (sheet.tracks.isEmpty()) {
        return Result<CueSheet>::err(Error::InvalidFormat,
            QStringLiteral("CUE %1 has no TRACK entries").arg(label));
    }

    qCDebug(lcCue) << "Parsed CUE" << label
                   << "tracks:" << sheet.tracks.size()
                   << "file:" << sheet.file;
    return Result<CueSheet>::ok(std::move(sheet));
}

Toc CueParser::tocFromSheet(const CueSheet& sheet, int totalAudioMs) {
    Toc toc;
    const int n = sheet.tracks.size();
    for (int i = 0; i < n; ++i) {
        const auto& t = sheet.tracks[i];
        TocEntry e;
        e.trackNumber = t.trackNumber > 0 ? t.trackNumber : i + 1;
        e.startSector = t.indexOneFrames >= 0 ? t.indexOneFrames : 0;
        e.title = t.title;

        long endFrames;
        if (i + 1 < n) {
            const auto& next = sheet.tracks[i + 1];
            endFrames = next.indexZeroFrames >= 0
                ? next.indexZeroFrames
                : (next.indexOneFrames >= 0 ? next.indexOneFrames : e.startSector);
        } else {
            // Last track — caller must provide total audio length to compute end.
            endFrames = totalAudioMs > 0
                ? (static_cast<long>(totalAudioMs) * 75 / 1000)
                : e.startSector;
        }
        e.endSector = endFrames;
        e.durationMs = framesToMs(endFrames - e.startSector);
        toc.entries.append(e);
        toc.totalDurationMs += e.durationMs;
    }
    return toc;
}

} // namespace soundshelf
