pragma Singleton
import QtQuick
import ScanCore

// Motion tokens. Durations collapse to zero when "Reduce motion" is on and
// stretch or shrink with the animation speed setting, so every animation in
// the app respects both by using these.
QtObject {
    // E-ink screens redraw slowly and ghost: no motion there either.
    readonly property bool reduced: App.settings.reducedMotion === true || App.settings.eink === true
    // Settings → Interface → Animation speed: a duration multiplier.
    readonly property real scale: ({ relaxed: 1.35, snappy: 0.7 })[App.settings.motionSpeed] || 1

    function ms(base) { return reduced ? 0 : Math.round(base * scale) }

    readonly property int instant: ms(90)
    readonly property int fast: ms(150)
    readonly property int normal: ms(240)
    readonly property int slow: ms(380)
    readonly property int page: ms(420)
    readonly property int hero: ms(520)

    // Cubic bezier control points for Easing.BezierSpline.
    readonly property var standard: [0.2, 0, 0, 1, 1, 1]
    readonly property var emphasized: [0.3, 0, 0, 1, 1, 1]
    readonly property var decelerate: [0.05, 0.7, 0.1, 1, 1, 1]
    readonly property var accelerate: [0.3, 0, 0.8, 0.15, 1, 1]
    readonly property var spring: [0.34, 1.36, 0.64, 1, 1, 1]

    function stagger(index) {
        return ms(Math.min(index, 16) * 24)
    }
}
