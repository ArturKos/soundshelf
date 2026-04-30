#include "soundshelf/core/Crossfader.hpp"
#include "soundshelf/core/PlayerEngine.hpp"

#include <QElapsedTimer>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcXf, "soundshelf.core.crossfade")

namespace soundshelf {

Crossfader::Crossfader(PlayerEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine)
{
    m_ramp.setInterval(20);  // 50 Hz volume updates
    connect(&m_ramp, &QTimer::timeout, this, &Crossfader::tickRamp);
    if (m_engine) enable();
}

Crossfader::~Crossfader() = default;

void Crossfader::setFadeMs(int fadeMs) {
    m_fadeMs = qMax(0, fadeMs);
    if (m_engine) m_engine->setCrossfadeMs(m_fadeMs);
}

void Crossfader::enable() {
    if (!m_engine) return;
    connect(m_engine, &PlayerEngine::positionChanged,
            this, &Crossfader::onPosition, Qt::UniqueConnection);
    connect(m_engine, &PlayerEngine::durationChanged,
            this, [this](int d) { m_durationMs = d; }, Qt::UniqueConnection);
    connect(m_engine, &PlayerEngine::trackChanged,
            this, &Crossfader::onTrackChanged, Qt::UniqueConnection);
}

void Crossfader::disable() {
    if (!m_engine) return;
    disconnect(m_engine, nullptr, this, nullptr);
    m_ramp.stop();
    m_fading = false;
}

void Crossfader::onTrackChanged() {
    m_fading = false;
    m_lastPosMs = 0;
    m_durationMs = m_engine ? m_engine->durationMs() : 0;
    m_ramp.stop();
}

void Crossfader::onPosition(int posMs) {
    m_lastPosMs = posMs;
    if (m_fadeMs <= 0 || m_durationMs <= 0 || m_fading) return;
    const int remaining = m_durationMs - posMs;
    if (remaining > 0 && remaining <= m_fadeMs) {
        m_fading = true;
        m_rampStartElapsedMs = posMs;
        qCDebug(lcXf) << "Begin fade-out, remaining" << remaining << "ms";
        m_ramp.start();
    }
}

void Crossfader::tickRamp() {
    if (!m_engine || m_fadeMs <= 0 || !m_fading) {
        m_ramp.stop();
        return;
    }
    const int into = qMax(0, m_lastPosMs - m_rampStartElapsedMs);
    if (into >= m_fadeMs) {
        // End of fade — engine will switch tracks via gapless / queue.
        m_engine->setVolume(0.0);
        m_ramp.stop();
        m_fading = false;
        return;
    }
    const double t = static_cast<double>(into) / static_cast<double>(m_fadeMs);
    // Equal-power curve sounds smoother than linear.
    const double gain = std::cos(t * 1.5707963267948966);  // π/2
    m_engine->setVolume(gain * 100.0);
}

} // namespace soundshelf
