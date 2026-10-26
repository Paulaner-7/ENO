import QtQuick
import Scancore

// Mouse-wheel and touchpad scrolling for a vertical Flickable (its parent),
// the same on every system. Wheels move in eased steps; touchpads (pixel
// deltas) move 2.5 times the fingers' travel, as GTK 4 does (browsers scale
// them up too; 1:1 felt slow), and glide on when the fingers lift. Both
// scale with Settings → Interface → Scroll speed. Touch screens fling as
// Android's lists do (Glide). Which device Qt reports is not trusted: on Wayland and with some
// mice a wheel arrives as a touchpad (or neither), and Qt's own Flickable
// wheel handling then moves a few pixels per notch.
WheelHandler {
    id: handler
    // Declared inside a Flickable (or ListView), a handler lands on its
    // content item; the Flickable is the one above.
    property Flickable flick: {
        for (let p = parent; p; p = p.parent)
            if (p instanceof Flickable) return p as Flickable
        return null
    }
    // Pixels per wheel notch at speed 1.
    property real wheelStep: 150
    readonly property real speed: Math.max(0.25, Number(App.settings.scrollSpeed) || 1)
    // A ListView's origin moves as delegates of unknown height are laid out.
    readonly property real minY: flick ? flick.originY - flick.topMargin : 0
    readonly property real maxY: flick ? Math.max(minY, flick.originY + flick.contentHeight - flick.height + flick.bottomMargin) : 0
    property real target: 0
    // Touchpad speed (pixels a second), for the fling when the fingers lift.
    property real velocity: 0
    property double lastTime: 0
    // GTK 4's MAGIC_SCROLL_FACTOR. macOS sends deltas already scaled.
    readonly property real touchpadGain: Qt.platform.os === "osx" ? 1 : 2.5
    // What was left of a touchpad glide that new scrolling stopped, to add
    // to the next one (GTK 4 accumulates the same way, up to 6 times).
    property real carried: 0

    property Glide glide: Glide { flick: handler.flick }

    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    // A WheelHandler keeps every event it is offered (accepted = false in
    // onWheel changes nothing), so it is only offered plain vertical ones:
    // Ctrl+wheel zooms and Shift+wheel scrolls rows elsewhere.
    acceptedModifiers: Qt.NoModifier
    orientation: Qt.Vertical

    function scrollTo(y, animated) {
        const clamped = Math.max(minY, Math.min(maxY, y))
        if (animated === false || Motion.reduced) {
            ease.stop()
            flick.contentY = clamped
            return
        }
        target = clamped
        ease.to = clamped
        ease.restart()
    }

    property NumberAnimation ease: NumberAnimation {
        target: handler.flick
        property: "contentY"
        duration: 320
        easing.type: Easing.OutCubic
    }

    onWheel: (event) => {
        if (!flick)
            return
        const pixels = event.pixelDelta.y
        // Linux and Windows touchpads report when the fingers lift but send
        // no momentum (macOS sends its own): fling at the fingers' speed.
        // GTK 4's kinetic scrolling: friction 4 (the speed falls by e⁻⁴ a
        // second), so a glide covers a quarter of the speed it starts at.
        if (event.phase === Qt.ScrollEnd) {
            if (Math.abs(velocity) > 300 && !Motion.reduced && Qt.platform.os !== "osx") {
                let v = velocity
                if (carried !== 0 && Math.sign(carried) === Math.sign(v)) v += carried
                v = Math.max(-6 * 5000, Math.min(6 * 5000, v))
                flick.flickDeceleration = 2 * Math.abs(v)
                if (flick.maximumFlickVelocity < Math.abs(v)) flick.maximumFlickVelocity = Math.abs(v)
                flick.flick(0, v)
            }
            velocity = 0
            carried = 0
            if (pixels === 0)
                return
        }
        const down = (pixels !== 0 ? pixels : event.angleDelta.y) < 0
        const base = ease.running ? target : flick.contentY
        if ((down && base >= maxY - 0.5) || (!down && base <= minY + 0.5))
            return
        if (pixels !== 0) {
            const gain = speed * touchpadGain
            const now = Date.now()
            const dt = Math.max(1, now - lastTime)
            velocity = dt > 100 ? 0 : velocity * 0.6 + pixels * gain / dt * 1000 * 0.4
            lastTime = now
            if (flick.flicking) {
                // flick() velocities point the other way from contentY's.
                carried = -flick.verticalVelocity
                flick.cancelFlick()
            }
            scrollTo(base - pixels * gain, false)
        } else
            scrollTo(base - event.angleDelta.y / 120 * wheelStep * speed)
    }
}
