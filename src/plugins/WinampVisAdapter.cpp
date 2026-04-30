#include "soundshelf/plugins/WinampVisAdapter.hpp"

#include <QLibrary>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWva, "soundshelf.plugins.winamp")

namespace soundshelf {

WinampVisAdapter::WinampVisAdapter(QObject* parent)
    : VisualizationPlugin(parent)
{}

WinampVisAdapter::~WinampVisAdapter() {
    unload();
}

Result<void> WinampVisAdapter::load(const QString& path) {
#ifndef Q_OS_WIN
    Q_UNUSED(path);
    return Result<void>::err(Error::DependencyMissing,
        QStringLiteral("Winamp visualisation plugins are Windows-only"));
#else
    unload();
    m_lib = std::make_unique<QLibrary>(path);
    if (!m_lib->load()) {
        return Result<void>::err(Error::FileAccessDenied,
            QStringLiteral("Cannot load %1: %2").arg(path, m_lib->errorString()));
    }
    using GetHeader = void* (*)();
    auto getHeader = reinterpret_cast<GetHeader>(
        m_lib->resolve("winampVisGetHeader"));
    if (!getHeader) {
        unload();
        return Result<void>::err(Error::InvalidFormat,
            QStringLiteral("%1 has no winampVisGetHeader entrypoint")
                .arg(QFileInfo(path).fileName()));
    }
    m_header = getHeader();
    if (!m_header) {
        unload();
        return Result<void>::err(Error::InvalidFormat,
            QStringLiteral("winampVisGetHeader returned null"));
    }
    // The header layout (winampVisHeader) starts with: int version,
    // const char* description, struct winampVisModule* (*getModule)(int).
    // Decoding it correctly requires the Winamp SDK headers — we save
    // the description and defer real module wiring until the SDK
    // bindings are vendored into third_party/.
    m_dllPath = path;
    m_id = QStringLiteral("winamp:%1").arg(QFileInfo(path).completeBaseName());
    m_displayName = QFileInfo(path).completeBaseName();
    m_description = QStringLiteral("Winamp visualisation: %1").arg(path);
    qCInfo(lcWva) << "Loaded Winamp plugin shell:" << m_displayName;
    return Result<void>::ok();
#endif
}

void WinampVisAdapter::unload() {
    if (m_lib) {
        m_lib->unload();
        m_lib.reset();
    }
    m_header = nullptr;
    m_module = nullptr;
}

void WinampVisAdapter::start(int sampleRate, int channels) {
    VisualizationPlugin::start(sampleRate, channels);
    // A complete implementation calls module->Init(module) here after
    // populating module->sRate / module->nCh.
}

void WinampVisAdapter::stop() {
    // Calls module->Quit(module).
}

void WinampVisAdapter::render(QPainter& /*painter*/,
                              const QRectF& /*area*/,
                              const QVector<float>& /*pcm*/,
                              const QVector<float>& /*spectrum*/) {
    // Winamp plugins paint into their own HWND — there is nothing for
    // SoundShelf to draw onto its painter. A complete implementation
    // would copy the floats into module->waveformData / spectrumData
    // (each is a 2 x 576 char[]) and then call module->Render(module).
}

} // namespace soundshelf
