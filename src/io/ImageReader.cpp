#include "soundshelf/io/ImageReader.hpp"
#include "soundshelf/io/TagInfo.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcImage, "soundshelf.io.image")

namespace soundshelf {

namespace {

QStringList audioCompanionExts() {
    return { QStringLiteral("flac"), QStringLiteral("wav"), QStringLiteral("ape"),
             QStringLiteral("wv"),   QStringLiteral("mp3"), QStringLiteral("ogg"),
             QStringLiteral("m4a"),  QStringLiteral("opus") };
}

} // namespace

ImageReader::ImageReader(QString path)
    : m_path(std::move(path))
{}

Result<QString> ImageReader::resolveCue(const QString& given) {
    QFileInfo fi(given);
    if (!fi.exists()) {
        return Result<QString>::err(Error::FileNotFound,
            QStringLiteral("Image source not found: %1").arg(given));
    }
    if (fi.suffix().compare(QLatin1String("cue"), Qt::CaseInsensitive) == 0) {
        return Result<QString>::ok(fi.absoluteFilePath());
    }
    // try sibling .cue
    QFileInfo sibling(fi.absoluteDir(), fi.completeBaseName() + QStringLiteral(".cue"));
    if (sibling.exists()) {
        return Result<QString>::ok(sibling.absoluteFilePath());
    }
    return Result<QString>::err(Error::InvalidFormat,
        QStringLiteral("No .cue companion for %1").arg(given));
}

Result<int> ImageReader::probeAudioDurationMs(const QString& audio) {
    auto t = TagInfo::fromFile(audio);
    if (!t) {
        // not fatal — a missing tag still leaves us with the CUE timing
        qCWarning(lcImage) << "Cannot read tags from" << audio << ":" << t.error().message;
        return Result<int>::ok(0);
    }
    return Result<int>::ok(t.value().durationMs);
}

Result<Toc> ImageReader::readToc() {
    auto cueResult = resolveCue(m_path);
    if (!cueResult) return Result<Toc>::err(cueResult.error().code, cueResult.error().message);
    m_cuePath = cueResult.value();

    CueParser parser;
    auto sheetResult = parser.parseFile(m_cuePath);
    if (!sheetResult) return Result<Toc>::err(sheetResult.error().code, sheetResult.error().message);
    m_sheet = sheetResult.value();

    QFileInfo cueFi(m_cuePath);

    if (m_sheet.isMultiFile()) {
        // Resolve each distinct FILE referenced by the sheet (in track order).
        QMap<QString, int> perFileDurationMs;
        QStringList processed;  // ordered set of distinct file keys already handled

        for (const auto& t : m_sheet.tracks) {
            if (processed.contains(t.file)) continue;
            processed.append(t.file);

            QFileInfo audioFi(cueFi.absoluteDir(), t.file);
            if (!audioFi.exists()) {
                // Per-file companion extension fallback using each file's own basename.
                QFileInfo afBase(t.file);
                bool found = false;
                for (const auto& ext : audioCompanionExts()) {
                    QFileInfo candidate(cueFi.absoluteDir(),
                        afBase.completeBaseName() + QLatin1Char('.') + ext);
                    if (candidate.exists()) { audioFi = candidate; found = true; break; }
                }
                if (!found) {
                    return Result<Toc>::err(Error::FileNotFound,
                        QStringLiteral("Audio file '%1' missing next to %2")
                            .arg(t.file, cueFi.fileName()));
                }
            }

            m_audioPaths.append(audioFi.absoluteFilePath());
            int ms = 0;
            if (auto d = probeAudioDurationMs(audioFi.absoluteFilePath()); d) ms = d.value();
            perFileDurationMs[t.file] = ms;
        }

        m_audioPath = m_audioPaths.isEmpty() ? QString() : m_audioPaths.first();

        Toc toc = CueParser::tocFromSheet(m_sheet, perFileDurationMs);
        qCInfo(lcImage) << "Multi-file image" << cueFi.fileName()
                        << "→" << m_audioPaths.size() << "containers,"
                        << toc.entries.size() << "tracks,"
                        << toc.totalDurationMs << "ms total";
        return Result<Toc>::ok(std::move(toc));
    }

    // Single-file path — unchanged behaviour.
    QFileInfo audioFi(cueFi.absoluteDir(), m_sheet.file);
    if (!audioFi.exists()) {
        // Fallback — search for any companion audio file with same basename.
        for (const auto& ext : audioCompanionExts()) {
            QFileInfo candidate(cueFi.absoluteDir(),
                cueFi.completeBaseName() + QLatin1Char('.') + ext);
            if (candidate.exists()) { audioFi = candidate; break; }
        }
    }
    if (!audioFi.exists()) {
        return Result<Toc>::err(Error::FileNotFound,
            QStringLiteral("Audio container '%1' missing next to %2")
                .arg(m_sheet.file, cueFi.fileName()));
    }
    m_audioPath = audioFi.absoluteFilePath();
    m_audioPaths = { m_audioPath };

    int totalMs = 0;
    if (auto d = probeAudioDurationMs(m_audioPath); d) totalMs = d.value();

    Toc toc = CueParser::tocFromSheet(m_sheet, totalMs);
    qCInfo(lcImage) << "Image" << cueFi.fileName()
                    << "→" << toc.entries.size() << "tracks,"
                    << totalMs << "ms total";
    return Result<Toc>::ok(std::move(toc));
}

} // namespace soundshelf
