pragma Singleton
import QtQuick
import Scancore

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

    // Flings. Android lets a finger throw a list at up to 8000 dp/s and lets
    // it glide out for a second or two; Qt's own limits (2500 px/s, braking
    // at 1500 px/s²) and the harder braking used before stopped a swipe
    // within a second of the finger lifting (#22). Settings → Interface →
    // Fling glide picks the braking.
    // Phones a little quicker still (users found 8000 and 2000 slow on
    // 120 Hz phones such as the realme GT 6T): a swipe goes further.
    readonly property real flickVelocity: Theme.touch ? 11000 : 5000
    readonly property real flickDeceleration: Theme.touch ? (({ short: 3000, long: 1000 })[App.settings.flingGlide] || 1600)
                                                          : (({ short: 3500, long: 1250 })[App.settings.flingGlide] || 2000)

    // Glide (touch screens): Android's OverScroller spline. A fling at v
    // dp/s travels friction·c·exp(r/(r−1)·ln(0.35·v/(friction·c))) dp, with
    // r = ln 0.78 / ln 0.9 and c the "physical coefficient" (gravity × inches
    // × 160 dp × 0.84); friction is Android's 0.015 by default, less for a
    // long glide and more for a short one (Settings → Interface).
    readonly property real flingMax: 8000
    readonly property real flingFriction: ({ short: 0.022, long: 0.010 })[App.settings.flingGlide] || 0.015
    function flingDistance(v) {
        const r = Math.log(0.78) / Math.log(0.9)
        const fc = flingFriction * 9.80665 * 39.37 * 160 * 0.84
        const l = Math.log(0.35 * Math.abs(v) / fc)
        return fc * Math.exp(r / (r - 1) * l)
    }

    function stagger(index) {
        return ms(Math.min(index, 16) * 24)
    }
}
