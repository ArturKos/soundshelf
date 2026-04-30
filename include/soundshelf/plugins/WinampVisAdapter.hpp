#pragma once

#include <QString>
#include <memory>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/plugins/VisualizationPlugin.hpp"

class QLibrary;

namespace soundshelf {

/**
 * @brief Adapter for classic Winamp visualisation plugins (`vis_*.dll`).
 *
 * Loads a Winamp 2.x-style DLL via `QLibrary`, locates the
 * `winampVisGetHeader` entrypoint, and forwards SoundShelf's PCM and
 * spectrum buffers into the plugin's `winampVisModule` callbacks.
 *
 * Notes on portability:
 *  - Winamp DLLs are Windows-only binaries. On Linux/macOS the
 *    adapter still exists but @ref load returns
 *    @c Error::DependencyMissing — calling sites should treat
 *    Winamp plugins as a Windows-only feature.
 *  - Many Winamp vis plugins paint into their own HWND. The classic
 *    in-process flow (e.g. MilkDrop) needs an embedded native window
 *    on Windows; that's accommodated by `winampVisModule.hwndParent`.
 *
 * @ref render forwards data into the loaded module but does not
 * paint anything onto SoundShelf's `QPainter` (Winamp plugins paint
 * themselves into their own window).
 */
class WinampVisAdapter : public VisualizationPlugin {
    Q_OBJECT
public:
    explicit WinampVisAdapter(QObject* parent = nullptr);
    ~WinampVisAdapter() override;

    /// Loads the DLL pointed to by @p path. Fails on non-Windows
    /// builds with @c Error::DependencyMissing.
    Result<void> load(const QString& path);

    /// Unloads the DLL.
    void unload();

    QString displayName() const override { return m_displayName; }
    QString id() const override { return m_id; }
    QString description() const override { return m_description; }
    Feed preferredFeed() const override { return WantBoth; }

    void start(int sampleRate, int channels) override;
    void stop() override;
    void render(QPainter& painter,
                const QRectF& area,
                const QVector<float>& pcm,
                const QVector<float>& spectrum) override;

private:
    std::unique_ptr<QLibrary> m_lib;
    void*       m_header = nullptr;     ///< winampVisHeader*
    void*       m_module = nullptr;     ///< winampVisModule*
    QString     m_dllPath;
    QString     m_displayName;
    QString     m_id;
    QString     m_description;
};

} // namespace soundshelf
