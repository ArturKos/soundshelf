#include "soundshelf/io/PlaylistExporter.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPle, "soundshelf.io.playlist.export")

namespace soundshelf {

namespace {

QString resolvePath(const Track& t, const QString& baseDir, bool relative) {
    if (!relative) return QFileInfo(t.filepath).absoluteFilePath();
    return QDir(baseDir).relativeFilePath(t.filepath);
}

} // namespace

PlaylistExporter::PlaylistExporter(QObject* parent) : QObject(parent) {}
PlaylistExporter::~PlaylistExporter() = default;

PlaylistExporter::Format PlaylistExporter::formatFromExtension(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("xspf")) return Format::XSPF;
    if (ext == QLatin1String("pls"))  return Format::PLS;
    return Format::M3U;
}

QString PlaylistExporter::toM3U(const QList<Track>& tracks, const QString& baseDir, bool relative) {
    QString out;
    out.reserve(tracks.size() * 80);
    out += QStringLiteral("#EXTM3U\n");
    for (const auto& t : tracks) {
        const int sec = t.durationMs > 0 ? (t.durationMs / 1000) : -1;
        const QString label = !t.title.isEmpty()
            ? (t.artist.isEmpty() ? t.title : QStringLiteral("%1 - %2").arg(t.artist, t.title))
            : QFileInfo(t.filepath).completeBaseName();
        out += QStringLiteral("#EXTINF:%1,%2\n").arg(sec).arg(label);
        out += resolvePath(t, baseDir, relative);
        out += QLatin1Char('\n');
    }
    return out;
}

QString PlaylistExporter::toPLS(const QList<Track>& tracks, const QString& baseDir, bool relative) {
    QString out;
    out += QStringLiteral("[playlist]\n");
    out += QStringLiteral("NumberOfEntries=%1\n").arg(tracks.size());
    int i = 1;
    for (const auto& t : tracks) {
        out += QStringLiteral("File%1=%2\n").arg(i).arg(resolvePath(t, baseDir, relative));
        const QString title = !t.title.isEmpty()
            ? (t.artist.isEmpty() ? t.title : QStringLiteral("%1 - %2").arg(t.artist, t.title))
            : QFileInfo(t.filepath).completeBaseName();
        out += QStringLiteral("Title%1=%2\n").arg(i).arg(title);
        out += QStringLiteral("Length%1=%2\n").arg(i).arg(t.durationMs > 0 ? t.durationMs / 1000 : -1);
        ++i;
    }
    out += QStringLiteral("Version=2\n");
    return out;
}

QString PlaylistExporter::toXSPF(const QList<Track>& tracks, const QString& baseDir, bool relative) {
    QString out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"));
    w.writeStartElement(QStringLiteral("playlist"));
    w.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));
    w.writeAttribute(QStringLiteral("xmlns"), QStringLiteral("http://xspf.org/ns/0/"));
    w.writeStartElement(QStringLiteral("trackList"));
    for (const auto& t : tracks) {
        w.writeStartElement(QStringLiteral("track"));
        const QString p = resolvePath(t, baseDir, relative);
        // For absolute paths emit a file:// URL; for relative leave bare.
        const QString loc = relative ? p : QUrl::fromLocalFile(p).toString();
        w.writeTextElement(QStringLiteral("location"), loc);
        if (!t.title.isEmpty())
            w.writeTextElement(QStringLiteral("title"), t.title);
        if (!t.artist.isEmpty())
            w.writeTextElement(QStringLiteral("creator"), t.artist);
        if (!t.album.isEmpty())
            w.writeTextElement(QStringLiteral("album"), t.album);
        if (t.trackNumber > 0)
            w.writeTextElement(QStringLiteral("trackNum"), QString::number(t.trackNumber));
        if (t.durationMs > 0)
            w.writeTextElement(QStringLiteral("duration"), QString::number(t.durationMs));
        w.writeEndElement(); // track
    }
    w.writeEndElement(); // trackList
    w.writeEndElement(); // playlist
    w.writeEndDocument();
    return out;
}

Result<void> PlaylistExporter::exportToFile(const QString& path,
                                            const QList<Track>& tracks,
                                            bool relativePaths,
                                            std::optional<Format> forceFormat) {
    const Format fmt = forceFormat.value_or(formatFromExtension(path));
    const QString baseDir = QFileInfo(path).absolutePath();
    QDir().mkpath(baseDir);

    QString text;
    switch (fmt) {
        case Format::M3U:  text = toM3U (tracks, baseDir, relativePaths); break;
        case Format::PLS:  text = toPLS (tracks, baseDir, relativePaths); break;
        case Format::XSPF: text = toXSPF(tracks, baseDir, relativePaths); break;
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return Result<void>::err(Error::FileAccessDenied, out.errorString());
    }
    out.write(text.toUtf8());
    if (!out.commit()) {
        return Result<void>::err(Error::FileAccessDenied, out.errorString());
    }
    qCInfo(lcPle) << "Exported" << tracks.size() << "tracks to" << path;
    return Result<void>::ok();
}

} // namespace soundshelf
