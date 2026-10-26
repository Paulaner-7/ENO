import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Switch with a springy knob.
Switch {
    id: control
    implicitWidth: 44
    implicitHeight: 26
    padding: 0
    hoverEnabled: true

    indicator: Rectangle {
        width: 44
        height: 26
        radius: Theme.retro ? 0 : 13
        color: control.checked ? Theme.accent : Theme.surface3
        border.width: control.checked ? 0 : 1
        border.color: Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Motion.normal } }
        Rectangle {
            width: control.pressed ? 24 : 20
            height: 20
            radius: Theme.retro ? 0 : 10
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? Theme.accentInk : (Theme.dark ? "#d8d8dc" : "#ffffff")
            Behavior on x { NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring } }
            Behavior on width { NumberAnimation { duration: Motion.fast } }
        }
    }
    contentItem: null
}
