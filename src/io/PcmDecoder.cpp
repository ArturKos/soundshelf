#include "soundshelf/io/PcmDecoder.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPcm, "soundshelf.io.pcm")

namespace soundshelf {

namespace {

/// Builds the ffmpeg argv that emits raw interleaved s16le on stdout.
QStringList decodeArgs(const QString& path, int targetRate, int channels) {
    return QStringList{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-i"), path,
        QStringLiteral("-vn"),
        QStringLiteral("-ac"), QString::number(channels),
        QStringLiteral("-ar"), QString::number(targetRate),
        QStringLiteral("-f"), QStringLiteral("s16le"),
        QStringLiteral("-acodec"), QStringLiteral("pcm_s16le"),
        QStringLiteral("-"),
    };
}

} // namespace

QString PcmDecoder::ffmpegPath() {
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

bool PcmDecoder::isAvailable() {
    return !ffmpegPath().isEmpty();
}

Result<void> PcmDecoder::streamS16(const QString& path,
                                   const SampleSink& sink,
                                   int targetRate,
                                   int channels,
                                   int* outTotalSamples) {
    if (channels < 1 || channels > 8 || targetRate <= 0) {
        return Result<void>::err(Error::InvalidArgument,
            QStringLiteral("invalid PCM parameters"));
    }
    const QString bin = ffmpegPath();
    if (bin.isEmpty()) {
        return Result<void>::err(Error::DependencyMissing,
            QStringLiteral("ffmpeg not found in PATH"));
    }
    if (!QFileInfo::exists(path)) {
        return Result<void>::err(Error::FileNotFound,
            QStringLiteral("Input not found: %1").arg(path));
    }

    QProcess proc;
    proc.setReadChannel(QProcess::StandardOutput);
    proc.start(bin, decodeArgs(path, targetRate, channels), QIODevice::ReadOnly);
    if (!proc.waitForStarted(3000)) {
        return Result<void>::err(Error::Unknown,
            QStringLiteral("ffmpeg failed to start: %1").arg(proc.errorString()));
    }

    const int frameBytes = 2 * channels;   // 16-bit * channels
    qint64 totalFrames = 0;
    QByteArray leftover;
    bool aborted = false;

    auto drainFullFrames = [&](bool& ok) -> bool {
        const int usableFrames = leftover.size() / frameBytes;
        if (usableFrames <= 0) return true;
        const auto* s = reinterpret_cast<const int16_t*>(leftover.constData());
        ok = sink(s, usableFrames);
        totalFrames += usableFrames;
        leftover.remove(0, usableFrames * frameBytes);
        return ok;
    };

    while (true) {
        if (proc.bytesAvailable() == 0) {
            if (proc.state() == QProcess::NotRunning) break;
            if (!proc.waitForReadyRead(60000)) {
                if (proc.state() == QProcess::NotRunning) break;
                proc.kill();
                proc.waitForFinished(2000);
                return Result<void>::err(Error::Unknown,
                    QStringLiteral("ffmpeg read timed out"));
            }
        }
        leftover += proc.readAllStandardOutput();
        bool sinkOk = true;
        if (!drainFullFrames(sinkOk) && !sinkOk) {
            aborted = true;
            proc.kill();
            proc.waitForFinished(2000);
            break;
        }
    }

    if (!aborted) {
        // flush whatever the process buffered after it exited
        leftover += proc.readAllStandardOutput();
        bool sinkOk = true;
        drainFullFrames(sinkOk);
        proc.waitForFinished(3000);
    }

    if (aborted) {
        return Result<void>::err(Error::OperationCancelled,
            QStringLiteral("decode aborted by sink"));
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return Result<void>::err(Error::InvalidFormat,
            QStringLiteral("ffmpeg decode failed (code %1): %2")
                .arg(proc.exitCode()).arg(err.left(200)));
    }

    if (outTotalSamples) *outTotalSamples = static_cast<int>(totalFrames);
    qCDebug(lcPcm) << "decoded" << path << totalFrames << "frames @"
                   << targetRate << "Hz x" << channels;
    return Result<void>::ok();
}

Result<PcmDecoder::PcmBuffer> PcmDecoder::decodeToS16(const QString& path,
                                                      int targetRate,
                                                      int channels) {
    PcmBuffer out;
    out.sampleRate = targetRate;
    out.channels   = channels;

    int total = 0;
    auto r = streamS16(path,
        [&out](const int16_t* samples, int frames) {
            out.s16le.append(reinterpret_cast<const char*>(samples),
                             qsizetype(frames) * out.channels * 2);
            return true;
        },
        targetRate, channels, &total);
    if (!r) return Result<PcmBuffer>::err(r.error().code, r.error().message);

    out.totalSamples = total;
    return Result<PcmBuffer>::ok(std::move(out));
}

} // namespace soundshelf
