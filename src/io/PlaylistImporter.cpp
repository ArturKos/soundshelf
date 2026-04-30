#include "soundshelf/io/PlaylistImporter.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QHash>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPli, "soundshelf.io.playlist.import")

namespace soundshelf {

namespace {

QString resolveAgainst(const QString& entry, const QString& baseDir) {
    if (entry.isEmpty()) return entry;
    const QUrl asUrl(entry);
    if (asUrl.scheme() == QLatin1String("http")
        || asUrl.scheme() == QLatin1String("https")
        || asUrl.scheme() == QLatin1String("ftp")) {
        return entry;
    }
    if (QFileInfo(entry).isAbsolute()) return entry;
    return QDir(baseDir).absoluteFilePath(entry);
}

} // namespace

PlaylistImporter::PlaylistImporter(QObject* parent) : QObject(parent) {}
PlaylistImporter::~PlaylistImporter() = default;

PlaylistImporter::Format PlaylistImporter::detectFormat(const QString& filename,
                                                       const QByteArray& head) {
    const QString lower = filename.toLower();
    if (lower.endsWith(QLatin1String(".xspf"))) return Format::XSPF;
    if (lower.endsWith(QLatin1String(".pls")))  return Format::PLS;
    if (lower.endsWith(QLatin1String(".m3u"))
        || lower.endsWith(QLatin1String(".m3u8"))) return Format::M3U;

    const QByteArray sniff = head.left(256).trimmed();
    if (sniff.startsWith("<?xml") || sniff.contains("<playlist")) return Format::XSPF;
    if (sniff.startsWith("[playlist]")) return Format::PLS;
    if (sniff.startsWith("#EXTM3U") || !sniff.isEmpty()) return Format::M3U;
    return Format::Unknown;
}

Result<QList<PlaylistImporter::Entry>> PlaylistImporter::importFile(const QString& path) {
    QFile f(path);
    if (!f.exists()) {
        return Result<QList<Entry>>::err(Error::FileNotFound,
            QStringLiteral("Playlist not found: %1").arg(path));
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return Result<QList<Entry>>::err(Error::FileAccessDenied, f.errorString());
    }
    const QByteArray bytes = f.readAll();
    const QString baseDir = QFileInfo(path).absolutePath();
    const Format fmt = detectFormat(path, bytes);

    const QString text = QString::fromUtf8(bytes);
    switch (fmt) {
        case Format::M3U:  return parseM3U(text, baseDir);
        case Format::PLS:  return parsePLS(text, baseDir);
        case Format::XSPF: return parseXSPF(text, baseDir);
        case Format::Unknown:
            return Result<QList<Entry>>::err(Error::InvalidFormat,
                QStringLiteral("Unrecognised playlist: %1").arg(path));
    }
    return Result<QList<Entry>>::err(Error::Unknown, QStringLiteral("unreachable"));
}

Result<QList<PlaylistImporter::Entry>>
PlaylistImporter::parseM3U(const QString& text, const QString& baseDir) {
    QList<Entry> out;
    QString pendingTitle;
    int pendingDur = -1;

    const auto lines = text.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(QLatin1Char('#'))) {
            if (line.startsWith(QLatin1String("#EXTINF:"))) {
                const QString tail = line.mid(8);
                const int comma = tail.indexOf(QLatin1Char(','));
                if (comma >= 0) {
                    bool ok = false;
                    const int sec = tail.left(comma).toInt(&ok);
                    if (ok) pendingDur = sec;
                    pendingTitle = tail.mid(comma + 1).trimmed();
                }
            }
            continue;
        }
        Entry e;
        e.path = resolveAgainst(line, baseDir);
        e.titleHint = pendingTitle;
        e.durationSec = pendingDur;
        out.append(e);
        pendingTitle.clear();
        pendingDur = -1;
    }
    qCDebug(lcPli) << "M3U imported" << out.size() << "entries";
    return Result<QList<Entry>>::ok(std::move(out));
}

Result<QList<PlaylistImporter::Entry>>
PlaylistImporter::parsePLS(const QString& text, const QString& baseDir) {
    QHash<int, Entry> byIndex;
    const auto lines = text.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('['))) continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();

        static const QRegularExpression re(QStringLiteral("^([A-Za-z]+)(\\d+)$"));
        const auto m = re.match(key);
        if (!m.hasMatch()) continue;
        const QString prefix = m.captured(1).toLower();
        const int idx = m.captured(2).toInt();
        Entry& e = byIndex[idx];
        if (prefix == QLatin1String("file"))    e.path = resolveAgainst(val, baseDir);
        else if (prefix == QLatin1String("title"))  e.titleHint = val;
        else if (prefix == QLatin1String("length")) e.durationSec = val.toInt();
    }
    QList<Entry> out;
    auto keys = byIndex.keys();
    std::sort(keys.begin(), keys.end());
    for (int k : keys) {
        if (!byIndex[k].path.isEmpty()) out.append(byIndex[k]);
    }
    qCDebug(lcPli) << "PLS imported" << out.size() << "entries";
    return Result<QList<Entry>>::ok(std::move(out));
}

Result<QList<PlaylistImporter::Entry>>
PlaylistImporter::parseXSPF(const QString& text, const QString& baseDir) {
    QList<Entry> out;
    QXmlStreamReader xr(text);
    Entry cur;
    bool inTrack = false;

    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement()) {
            const auto name = xr.name();
            if (name == QLatin1String("track")) { inTrack = true; cur = Entry(); }
            else if (inTrack && name == QLatin1String("location")) {
                const QString loc = xr.readElementText().trimmed();
                cur.path = resolveAgainst(QUrl(loc).toLocalFile().isEmpty() ? loc
                                                  : QUrl(loc).toLocalFile(),
                                          baseDir);
            } else if (inTrack && name == QLatin1String("title")) {
                cur.titleHint = xr.readElementText().trimmed();
            } else if (inTrack && name == QLatin1String("duration")) {
                bool ok = false;
                const int ms = xr.readElementText().toInt(&ok);
                if (ok) cur.durationSec = ms / 1000;
            }
        } else if (xr.isEndElement() && xr.name() == QLatin1String("track")) {
            inTrack = false;
            if (!cur.path.isEmpty()) out.append(cur);
        }
    }
    if (xr.hasError()) {
        return Result<QList<Entry>>::err(Error::InvalidFormat,
            QStringLiteral("XSPF parse error: %1").arg(xr.errorString()));
    }
    qCDebug(lcPli) << "XSPF imported" << out.size() << "entries";
    return Result<QList<Entry>>::ok(std::move(out));
}

} // namespace soundshelf
