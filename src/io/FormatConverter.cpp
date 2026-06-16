#include "soundshelf/io/FormatConverter.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFmt, "soundshelf.io.format")

namespace soundshelf {

struct FormatConverter::Impl {
    QProcess proc;
    int totalDurationSec = 0;     ///< parsed from ffmpeg's `Duration:` line
    int lastReportedPct = -1;
};

FormatConverter::FormatConverter(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>())
{
    d->proc.setProcessChannelMode(QProcess::MergedChannels);

    connect(&d->proc, &QProcess::readyReadStandardOutput,
            this, [this]() {
        const QByteArray chunk = d->proc.readAllStandardOutput();
        const QString s = QString::fromUtf8(chunk);

        static const QRegularExpression durRe(
            QStringLiteral("Duration:\\s*(\\d+):(\\d+):(\\d+)"));
        if (auto m = durRe.match(s); m.hasMatch()) {
            d->totalDurationSec = m.captured(1).toInt() * 3600
                                + m.captured(2).toInt() * 60
                                + m.captured(3).toInt();
        }

        static const QRegularExpression timeRe(
            QStringLiteral("time=\\s*(\\d+):(\\d+):(\\d+)"));
        if (auto m = timeRe.match(s); m.hasMatch() && d->totalDurationSec > 0) {
            const int cur = m.captured(1).toInt() * 3600
                          + m.captured(2).toInt() * 60
                          + m.captured(3).toInt();
            const int pct = qBound(0, (cur * 100) / d->totalDurationSec, 100);
            if (pct != d->lastReportedPct) {
                d->lastReportedPct = pct;
                emit progress(pct);
            }
        }
    });

    connect(&d->proc, &QProcess::finished,
            this, [this](int exitCode, QProcess::ExitStatus status) {
        const bool ok = (status == QProcess::NormalExit) && exitCode == 0;
        const QString msg = ok
            ? QStringLiteral("ok")
            : QStringLiteral("ffmpeg exited with code %1").arg(exitCode);
        qCInfo(lcFmt) << "Conversion finished:" << msg;
        emit finished(ok, msg);
    });
}

FormatConverter::~FormatConverter() {
    if (d && d->proc.state() != QProcess::NotRunning) {
        d->proc.kill();
        d->proc.waitForFinished(2000);
    }
}

QString FormatConverter::ffmpegPath() {
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

bool FormatConverter::isAvailable() {
    return !ffmpegPath().isEmpty();
}

QString FormatConverter::extensionForFormat(Format f) {
    switch (f) {
        case Format::Mp3V0:    return QStringLiteral("mp3");
        case Format::Mp3_320:  return QStringLiteral("mp3");
        case Format::OggVorbis: return QStringLiteral("ogg");
        case Format::Opus_128: return QStringLiteral("opus");
        case Format::Aac_256:  return QStringLiteral("m4a");
        case Format::Flac:     return QStringLiteral("flac");
        case Format::WavPcm16: return QStringLiteral("wav");
    }
    return QStringLiteral("flac"); // unreachable, satisfies MSVC
}

QStringList FormatConverter::buildArguments(const Job& job) {
    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-nostdin");
    if (job.overwrite) args << QStringLiteral("-y"); else args << QStringLiteral("-n");
    args << QStringLiteral("-i") << job.input;
    args << QStringLiteral("-map_metadata") << QStringLiteral("0");
    args << QStringLiteral("-vn"); // strip video, cover handled separately

    switch (job.format) {
        case Format::Mp3V0:
            args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame")
                 << QStringLiteral("-q:a") << QStringLiteral("0");
            break;
        case Format::Mp3_320:
            args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame")
                 << QStringLiteral("-b:a") << QStringLiteral("320k");
            break;
        case Format::OggVorbis:
            args << QStringLiteral("-c:a") << QStringLiteral("libvorbis")
                 << QStringLiteral("-q:a") << QStringLiteral("6");
            break;
        case Format::Opus_128:
            args << QStringLiteral("-c:a") << QStringLiteral("libopus")
                 << QStringLiteral("-b:a") << QStringLiteral("128k");
            break;
        case Format::Aac_256:
            args << QStringLiteral("-c:a") << QStringLiteral("aac")
                 << QStringLiteral("-b:a") << QStringLiteral("256k");
            break;
        case Format::Flac:
            args << QStringLiteral("-c:a") << QStringLiteral("flac")
                 << QStringLiteral("-compression_level") << QStringLiteral("8");
            break;
        case Format::WavPcm16:
            args << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le");
            break;
    }

    if (job.samplerateOverride > 0) {
        args << QStringLiteral("-ar") << QString::number(job.samplerateOverride);
    }
    if (job.channelsOverride > 0) {
        args << QStringLiteral("-ac") << QString::number(job.channelsOverride);
    }
    args << job.output;
    return args;
}

Result<void> FormatConverter::start(const Job& job) {
    if (d->proc.state() != QProcess::NotRunning) {
        return Result<void>::err(Error::DeviceNotReady,
            QStringLiteral("Conversion already in progress"));
    }
    const QString bin = ffmpegPath();
    if (bin.isEmpty()) {
        return Result<void>::err(Error::DependencyMissing,
            QStringLiteral("ffmpeg not found in PATH"));
    }
    if (!QFileInfo::exists(job.input)) {
        return Result<void>::err(Error::FileNotFound,
            QStringLiteral("Input not found: %1").arg(job.input));
    }
    if (!job.overwrite && QFileInfo::exists(job.output)) {
        return Result<void>::err(Error::InvalidArgument,
            QStringLiteral("Output exists: %1 (set overwrite)").arg(job.output));
    }

    d->totalDurationSec = 0;
    d->lastReportedPct = -1;
    const QStringList args = buildArguments(job);
    qCInfo(lcFmt) << "ffmpeg" << args;
    d->proc.start(bin, args);

    if (!d->proc.waitForStarted(3000)) {
        return Result<void>::err(Error::Unknown,
            QStringLiteral("ffmpeg failed to start: %1").arg(d->proc.errorString()));
    }
    return Result<void>::ok();
}

bool FormatConverter::isRunning() const {
    return d->proc.state() != QProcess::NotRunning;
}

void FormatConverter::cancel() {
    if (d->proc.state() != QProcess::NotRunning) {
        qCWarning(lcFmt) << "Conversion cancelled";
        d->proc.terminate();
        if (!d->proc.waitForFinished(2000)) {
            d->proc.kill();
        }
    }
}

} // namespace soundshelf
