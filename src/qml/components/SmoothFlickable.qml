import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Vertical Flickable with eased mouse-wheel scrolling. Touchpads keep their
// native pixel scrolling.
Flickable {
    id: flick
    property real wheelStep: 120
    property real target: 0
    readonly property real maxY: Math.max(0, contentHeight - height + topMargin + bottomMargin) - topMargin

    onContentYChanged: if (!wheelAnim.running) target = contentY

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    maximumFlickVelocity: 4500
    flickDeceleration: 3500
    ScrollBar.vertical: ScScrollBar {}

    function scrollTo(y, animated) {
        const clamped = Math.max(-topMargin, Math.min(maxY, y))
        if (animated === false || Motion.reduced) {
            wheelAnim.stop()
            target = clamped
            contentY = clamped
            return
        }
        target = clamped
        wheelAnim.to = clamped
        wheelAnim.restart()
    }

    NumberAnimation {
        id: wheelAnim
        target: flick
        property: "contentY"
        duration: 360
        easing.type: Easing.OutCubic
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse
        onWheel: (event) => {
            if (event.modifiers & Qt.ControlModifier) {
                event.accepted = false
                return
            }
            if (event.phase !== Qt.NoScrollPhase || event.pixelDelta.y !== 0) {
                wheelAnim.stop()
                const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y
                flick.contentY = Math.max(-flick.topMargin, Math.min(flick.maxY, flick.contentY - dy))
                return
            }
            const base = wheelAnim.running ? flick.target : flick.contentY
            flick.scrollTo(base - event.angleDelta.y / 120 * flick.wheelStep * 1.25)
        }
    }
}
