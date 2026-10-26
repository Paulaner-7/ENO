import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Floating button that brings a long page back to its top. Pops in once the
// page has scrolled more than a screen and a half.
AbstractButton {
    id: button
    property Flickable flick
    readonly property bool shown: flick !== null && flick.contentY - flick.originY > flick.height * 1.5

    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.margins: Theme.compact ? 16 : 28
    width: 44
    height: 44
    z: 5
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    enabled: shown
    opacity: shown ? 1 : 0
    scale: shown ? (pressed ? 0.9 : 1) : 0.6
    visible: opacity > 0.01
    Behavior on opacity { NumberAnimation { duration: Motion.normal } }
    Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutBack } }

    onClicked: {
        if (typeof flick.scrollTo === "function") {
            flick.scrollTo(-flick.topMargin)
            return
        }
        toTop.from = flick.contentY
        toTop.to = flick.originY - flick.topMargin
        toTop.restart()
    }
    NumberAnimation {
        id: toTop
        target: button.flick
        property: "contentY"
        duration: Motion.slow + 120
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Motion.emphasized
    }

    Shadow { z: -2; target: face; lift: button.hovered ? 1 : 0.4; strength: Theme.dark ? 0.8 : 0.25 }
    background: LiquidGlass {
        id: face
        radius: width / 2
        backdropItem: button.flick
        interactive: true
        externalHovered: button.hovered
        externalPressed: button.pressed
        trackPointer: false
        iconName: "arrow-up"
        iconSize: 18
        iconColor: Theme.text
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: button.hovered ? -2 : 0
            name: "arrow-up"
            size: 18
            color: Theme.text
            Behavior on anchors.verticalCenterOffset { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
        }
    }
    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: qsTr("Back to top")
}
