#include "soundshelf/io/DiscRipper.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QEventLoop>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRip, "soundshelf.io.rip")

namespace soundshelf {

namespace {

QString sanitiseFilename(QString s) {
    static const QString bad = QStringLiteral(R"(<>:"/\|?*)");
    for (QChar c : bad) s.replace(c, QLatin1Char('_'));
    return s.trimmed().isEmpty() ? QStringLiteral("untitled") : s;
}

} // namespace

struct DiscRipper::Impl {
    Job job;
    std::unique_ptr<CDDAReader> reader;
    std::unique_ptr<FormatConverter> converter;
    Toc toc;
    bool running = false;
    bool cancelRequested = false;
};

DiscRipper::DiscRipper(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {}

DiscRipper::~DiscRipper() = default;

QString DiscRipper::applyPattern(const QString& pattern,
                                 int trackNumber,
                                 const QString& title,
                                 const QString& artist,
                                 const QString& album,
                                 int year,
                                 const QString& ext) {
    QString s = pattern;
    s.replace(QStringLiteral("%track%"),
              QStringLiteral("%1").arg(trackNumber, 2, 10, QLatin1Char('0')));
    s.replace(QStringLiteral("%title%"),  sanitiseFilename(title));
    s.replace(QStringLiteral("%artist%"), sanitiseFilename(artist));
    s.replace(QStringLiteral("%album%"),  sanitiseFilename(album));
    s.replace(QStringLiteral("%year%"),   QString::number(year));
    s.replace(QStringLiteral("%ext%"),    ext);
    return s;
}

bool DiscRipper::isRunning() const { return d->running; }

void DiscRipper::cancel() {
    if (d->running) d->cancelRequested = true;
}

Result<void> DiscRipper::start(const Job& job) {
    if (d->running) {
        return Result<void>::err(Error::DeviceNotReady,
            QStringLiteral("Rip already in progress"));
    }
    d->job = job;
    d->cancelRequested = false;

    QDir().mkpath(job.outputDir);

    d->reader = std::make_unique<CDDAReader>(job.device);
    auto tocR = d->reader->readToc();
    if (!tocR) return Result<void>::err(tocR.error().code, tocR.error().message);
    d->toc = tocR.value();

    if (d->toc.entries.isEmpty()) {
        return Result<void>::err(Error::InvalidFormat,
            QStringLiteral("Empty disc TOC"));
    }

    d->converter = std::make_unique<FormatConverter>(this);
    d->running = true;

    // Drive the loop on the event loop's next tick so the caller sees
    // start() return successfully before signals start firing.
    QTimer::singleShot(0, this, [this]() {
        for (int i = 0; i < d->toc.entries.size(); ++i) {
            if (d->cancelRequested) break;
            const auto& e = d->toc.entries[i];
            emit trackStarted(e.trackNumber);

            const QString wavName = applyPattern(d->job.filenamePattern,
                e.trackNumber, e.title, d->job.discArtist, d->job.discTitle,
                d->job.year, QStringLiteral("wav"));
            const QString wavPath = QDir(d->job.outputDir).filePath(wavName);

            qCInfo(lcRip) << "Ripping track" << e.trackNumber << "→" << wavPath;
            auto rr = d->reader->ripTrackToWav(e.trackNumber, wavPath);
            if (!rr) {
                emit trackFinished(e.trackNumber, false, rr.error().message);
                continue;
            }

            if (!d->job.convertAfter) {
                emit trackFinished(e.trackNumber, true, wavPath);
                continue;
            }

            // Convert wav → target format (synchronous, single ffmpeg per drive).
            FormatConverter::Job cj;
            cj.input = wavPath;
            const QString outExt = [&]{
                using F = FormatConverter::Format;
                switch (d->job.targetFormat) {
                    case F::Mp3V0: case F::Mp3_320: return QStringLiteral("mp3");
                    case F::OggVorbis: return QStringLiteral("ogg");
                    case F::Opus_128:  return QStringLiteral("opus");
                    case F::Aac_256:   return QStringLiteral("m4a");
                    case F::Flac:      return QStringLiteral("flac");
                    case F::WavPcm16:  return QStringLiteral("wav");
                }
                return QStringLiteral("flac");
            }();
            const QString outName = applyPattern(d->job.filenamePattern,
                e.trackNumber, e.title, d->job.discArtist, d->job.discTitle,
                d->job.year, outExt);
            cj.output = QDir(d->job.outputDir).filePath(outName);
            cj.format = d->job.targetFormat;
            cj.overwrite = true;

            auto cr = d->converter->start(cj);
            if (!cr) {
                emit trackFinished(e.trackNumber, false, cr.error().message);
                continue;
            }
            QEventLoop loop;
            bool localOk = true;
            QObject::connect(d->converter.get(), &FormatConverter::finished,
                             &loop, [&](bool ok, const QString&) {
                                 localOk = ok;
                                 loop.quit();
                             }, Qt::SingleShotConnection);
            loop.exec();

            if (d->job.deleteWavAfterConvert && localOk) {
                QFile::remove(wavPath);
            }
            emit trackFinished(e.trackNumber, localOk, cj.output);
        }
        d->running = false;
        emit allFinished(!d->cancelRequested);
    });

    return Result<void>::ok();
}

} // namespace soundshelf
