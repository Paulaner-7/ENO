import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Thin overlay scrollbar that widens on hover.
ScrollBar {
    id: bar
    hoverEnabled: true
    minimumSize: 0.06
    implicitWidth: hovered || pressed ? 10 : 6
    padding: 2
    Behavior on implicitWidth { NumberAnimation { duration: Motion.fast } }

    contentItem: Rectangle {
        radius: width / 2
        color: bar.pressed ? Theme.textMuted : Theme.alpha(Theme.text, bar.hovered ? 0.35 : 0.2)
        opacity: bar.policy === ScrollBar.AlwaysOn || bar.active || bar.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Motion.slow } }
    }
    background: null
}
