import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Labelled text input with a hint or problem line underneath.
Column {
    id: field
    property alias text: input.text
    property alias placeholder: input.placeholderText
    property alias echoMode: input.echoMode
    property alias inputMethodHints: input.inputMethodHints
    property string label
    property string problem
    property string hint
    signal accepted()

    function focusInput() { input.forceActiveFocus() }

    spacing: 6
    width: parent ? parent.width : 320

    Text {
        text: field.label
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.textSm
        font.weight: Font.Medium
    }
    TextField {
        id: input
        width: parent.width
        implicitHeight: 46
        leftPadding: 14
        rightPadding: reveal.visible ? 44 : 14
        color: Theme.text
        placeholderTextColor: Theme.textFaint
        selectionColor: Theme.alpha(Theme.accent, 0.35)
        font.family: Theme.fontSans
        font.pixelSize: Theme.textMd
        selectByMouse: true
        onAccepted: field.accepted()
        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surface2
            border.width: 1
            border.color: field.problem ? Theme.danger : input.activeFocus ? Theme.alpha(Theme.accent, 0.7) : Theme.border
            Behavior on border.color { ColorAnimation { duration: Motion.fast } }
        }
        IconButton {
            id: reveal
            visible: field.echoMode === TextInput.Password || reveal.showing
            property bool showing: false
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            diameter: 32
            iconSize: 16
            iconName: showing ? "eye-off" : "eye"
            tip: showing ? qsTr("Hide password") : qsTr("Show password")
            onClicked: {
                showing = !showing
                input.echoMode = showing ? TextInput.Normal : TextInput.Password
            }
        }
    }
    Text {
        visible: text.length > 0
        width: parent.width
        text: field.problem || field.hint
        color: field.problem ? Theme.danger : Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs + 1
        wrapMode: Text.Wrap
    }
}
