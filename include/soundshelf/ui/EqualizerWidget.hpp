#pragma once

#include <QWidget>
#include <QVector>

class QSlider;
class QComboBox;
class QCheckBox;

namespace soundshelf {

class PlayerEngine;

/**
 * @brief 10-band graphic equaliser with vertical sliders and presets.
 *
 * Reads the band centre frequencies and gain range from
 * @ref PlayerEngine. Each slider drives `PlayerEngine::setEqualizerBand`
 * directly. Presets are loaded from `resources/eq_presets/*.json`
 * (`flat`, `rock`, `classical`, `electronic`, `bass_boost`, ...).
 */
class EqualizerWidget : public QWidget {
    Q_OBJECT
public:
    explicit EqualizerWidget(QWidget* parent = nullptr);
    ~EqualizerWidget() override;

    void attachEngine(PlayerEngine* engine);

    /// Sets all bands to @p gainsDb. Length must equal `EQ_BANDS`.
    void setBandGains(const QVector<double>& gainsDb);

    QVector<double> bandGains() const;

private:
    void setupUi();
    void loadPresetList();

    PlayerEngine*     m_engine = nullptr;
    QVector<QSlider*> m_sliders;
    QComboBox*        m_presetBox = nullptr;
    QCheckBox*        m_enable = nullptr;
};

} // namespace soundshelf
