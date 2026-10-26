import QtQuick
import QtQuick.Controls.Basic
import Scancore

// An action's keys as key caps. Click to record new ones: the next key press
// (with any modifiers) becomes the shortcut, Backspace leaves the action
// without one, and Esc or clicking elsewhere keeps what it had.
AbstractButton {
    id: recorder
    property var labels: []
    property bool recording: false
    signal captured(string sequence)
    signal cleared()

    implicitWidth: Math.max(120, caps.implicitWidth + 24)
    implicitHeight: 34
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    onClicked: {
        recording = !recording
        if (recording) forceActiveFocus()
    }
    onActiveFocusChanged: if (!activeFocus) recording = false

    // While recording, keys are ours, not the app's shortcuts.
    Keys.onShortcutOverride: (event) => event.accepted = recording
    Keys.onPressed: (event) => {
        if (!recording) return
        event.accepted = true
        if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
            recording = false
            return
        }
        if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && event.modifiers === Qt.NoModifier) {
            recording = false
            cleared()
            return
        }
        const sequence = Customize.keySequence(event.key, event.modifiers)
        if (!sequence) return // a modifier on its own: wait for the key
        recording = false
        captured(sequence)
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: recorder.recording ? Theme.alpha(Theme.accent, 0.12) : recorder.hovered ? Theme.surface3 : Theme.surface2
        border.width: recorder.recording || recorder.visualFocus ? 2 : 1
        border.color: recorder.recording ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Motion.fast } }
        SequentialAnimation on opacity {
            running: recorder.recording && !Motion.reduced
            loops: Animation.Infinite
            alwaysRunToEnd: true
            NumberAnimation { to: 0.6; duration: 600; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1; duration: 600; easing.type: Easing.InOutSine }
        }
    }

    contentItem: Item {
        Row {
            id: caps
            anchors.centerIn: parent
            spacing: 6
            visible: !recorder.recording
            Text {
                visible: recorder.labels.length === 0
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("None")
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
            }
            Repeater {
                model: recorder.labels
                Rectangle {
                    required property string modelData
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24
                    width: capText.implicitWidth + 16
                    radius: 6
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.borderStrong
                    Text {
                        id: capText
                        anchors.centerIn: parent
                        text: modelData
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textXs + 1
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
        Text {
            anchors.centerIn: parent
            visible: recorder.recording
            text: qsTr("Press keys…")
            color: Theme.accent
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
        }
    }
}
