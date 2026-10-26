import QtQuick
import QtQuick.Controls.Basic
import Scancore

Slider {
    id: control
    implicitWidth: 200
    implicitHeight: 26
    hoverEnabled: true

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: control.hovered || control.pressed ? 6 : 4
        radius: height / 2
        color: Theme.surface3
        Behavior on height { NumberAnimation { duration: Motion.fast } }
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: Theme.accent
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.pressed ? 18 : 16
        height: width
        radius: width / 2
        color: Theme.dark ? "#ffffff" : Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
        Behavior on width { NumberAnimation { duration: Motion.fast } }
    }
}
