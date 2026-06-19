#pragma once

#include <QColor>
#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace soundshelf {

/// Colour scheme shared by every visualisation (spectrum bars, oscilloscope,
/// waveform). Rainbow ignores @ref VisualStyle::accent; the others derive their
/// colours from it.
enum class VisScheme { Rainbow, Solid, Neon, Phosphor, Amber, Ice };

/// Active visual style: a scheme plus an accent colour for the accent-based
/// schemes. Persisted in QSettings and shared process-wide so every visualiser
/// (the Spectrum tab and the transport waveform) stays in sync.
struct VisualStyle {
    VisScheme scheme = VisScheme::Rainbow;
    QColor    accent = QColor(57, 255, 20);   ///< used by Solid / Neon
};

inline QString visSchemeToString(VisScheme s) {
    switch (s) {
        case VisScheme::Rainbow:  return QStringLiteral("rainbow");
        case VisScheme::Neon:     return QStringLiteral("neon");
        case VisScheme::Phosphor: return QStringLiteral("phosphor");
        case VisScheme::Amber:    return QStringLiteral("amber");
        case VisScheme::Ice:      return QStringLiteral("ice");
        case VisScheme::Solid:    default: return QStringLiteral("solid");
    }
}

inline VisScheme visSchemeFromString(const QString& s) {
    if (s == QLatin1String("rainbow"))  return VisScheme::Rainbow;
    if (s == QLatin1String("neon"))     return VisScheme::Neon;
    if (s == QLatin1String("phosphor")) return VisScheme::Phosphor;
    if (s == QLatin1String("amber"))    return VisScheme::Amber;
    if (s == QLatin1String("ice"))      return VisScheme::Ice;
    return VisScheme::Solid;
}

/// Fixed accent for the preset schemes (so Phosphor/Amber/Ice render without a
/// user-chosen accent); Rainbow/Solid/Neon use the style's own accent.
inline QColor visEffectiveAccent(const VisualStyle& s) {
    switch (s.scheme) {
        case VisScheme::Phosphor: return QColor(57, 255, 20);
        case VisScheme::Amber:    return QColor(255, 176, 0);
        case VisScheme::Ice:      return QColor(80, 200, 255);
        default:                  return s.accent;
    }
}

/// Colour for a visualisation element at normalised position @p t in [0,1]
/// (e.g. bar index across the spectrum, or x across the waveform) and intensity
/// @p level in [0,1] (e.g. bar height). Rainbow sweeps the hue across @p t; the
/// accent schemes keep a fixed hue and modulate brightness by @p level.
inline QColor visColor(const VisualStyle& s, qreal t, qreal level = 1.0) {
    t     = qBound(0.0, t, 1.0);
    level = qBound(0.0, level, 1.0);
    if (s.scheme == VisScheme::Rainbow) {
        const int hue = (int(285.0 * (1.0 - t))) % 360;   // red(0) low → violet(285) high
        return QColor::fromHsv(hue, 255, int(110 + 145 * level));
    }
    QColor a = visEffectiveAccent(s);
    qreal h = a.hsvHueF();
    if (h < 0.0) h = 0.0;                       // achromatic accent → treat as red hue
    const qreal sat = qMax<qreal>(0.35, a.hsvSaturationF());
    return QColor::fromHsvF(h, sat, qBound(0.0, 0.30 + 0.70 * level, 1.0));
}

/// True when the scheme wants an extra glow pass (purely cosmetic).
inline bool visWantsGlow(const VisualStyle& s) { return s.scheme == VisScheme::Neon; }

/// Process-wide current style, lazily loaded from QSettings on first use.
inline VisualStyle& currentVisualStyle() {
    static VisualStyle style = [] {
        QSettings st;
        VisualStyle v;
        v.scheme = visSchemeFromString(
            st.value(QStringLiteral("visual/scheme"), QStringLiteral("rainbow")).toString());
        const QString c = st.value(QStringLiteral("visual/accent")).toString();
        if (!c.isEmpty() && QColor(c).isValid()) v.accent = QColor(c);
        return v;
    }();
    return style;
}

/// Updates the shared style and persists it. Visualisers pick it up on their
/// next repaint tick.
inline void setCurrentVisualStyle(const VisualStyle& v) {
    currentVisualStyle() = v;
    QSettings st;
    st.setValue(QStringLiteral("visual/scheme"), visSchemeToString(v.scheme));
    st.setValue(QStringLiteral("visual/accent"), v.accent.name());
}

} // namespace soundshelf
