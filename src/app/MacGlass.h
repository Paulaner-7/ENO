#pragma once

#include <QColor>
#include <QQuickWindow>
#include <QVariantMap>

namespace sc::macglass {

// Queries macOS 27's native NSGlassEffectView / CABackdropLayer ("glassBackground")
// for its optical parameters (refraction amount/height, saturation, specular highlight
// angle/spread/amount, interactive glass support).
#ifdef Q_OS_MACOS
QVariantMap opticalMetrics();
void syncWindowGlass(QQuickWindow *window, bool dark, const QColor &tint, bool clearStyle = false);
#else
inline QVariantMap opticalMetrics() {
    return {
        {QStringLiteral("nativeAvailable"), false},
        {QStringLiteral("interactiveAvailable"), false},
        {QStringLiteral("refractionAmount"), -30.0},
        {QStringLiteral("refractionHeight"), 15.0},
        {QStringLiteral("clearRefractionAmount"), -39.0},
        {QStringLiteral("clearRefractionHeight"), 20.0},
        {QStringLiteral("blurRadius"), 5.0},
        {QStringLiteral("clearBlurRadius"), 5.833333},
        {QStringLiteral("darkSaturation"), 1.3},
        {QStringLiteral("lightSaturation"), 1.2},
        {QStringLiteral("highlightAngle"), 1.5707963267948966},
        {QStringLiteral("highlightSpread"), 2.0943951023931953},
        {QStringLiteral("highlightAmount"), 0.5},
        {QStringLiteral("clearHighlightSpread"), 1.3089969389957472},
        {QStringLiteral("clearHighlightAmount"), 0.4},
        {QStringLiteral("bleedAmount"), 21.0},
    };
}
inline void syncWindowGlass(QQuickWindow *, bool, const QColor &, bool = false) {}
#endif

} // namespace sc::macglass
