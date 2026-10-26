import QtQuick
import Scancore

// Loading placeholder with a light sweep moving across it.
Rectangle {
    id: root
    color: Theme.surface2
    radius: Theme.radiusMd
    clip: true

    Rectangle {
        id: sweep
        width: Math.max(root.width * 0.6, 80)
        height: root.height * 3
        anchors.verticalCenter: parent.verticalCenter
        rotation: 18
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.alpha(Theme.text, Theme.dark ? 0.05 : 0.07) }
            GradientStop { position: 1.0; color: "transparent" }
        }
        NumberAnimation on x {
            from: -sweep.width * 1.5
            to: root.width + sweep.width
            duration: 1400
            loops: Animation.Infinite
            running: root.visible && !Motion.reduced
            easing.type: Easing.InOutQuad
        }
    }
}
