import QtQuick
import Scancore

// An access code typed into one box per character. One hidden input takes
// the typing (and pasting), so backspace and phone keyboards just work.
Item {
    id: boxes
    property int length: 6
    property alias text: input.text
    readonly property bool complete: input.text.length === length
    signal accepted()

    function focusInput() { input.forceActiveFocus() }

    implicitWidth: length * 48 + (length - 1) * 8
    implicitHeight: 54

    TextInput {
        id: input
        width: 1
        height: 1
        opacity: 0
        // Room for a pasted dash or spaces, removed below.
        maximumLength: boxes.length + 4
        inputMethodHints: Qt.ImhPreferUppercase | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhLatinOnly
        validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9 -]*/ }
        // Upper case, without the dash or spaces people paste with it.
        onTextEdited: {
            const clean = text.toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, boxes.length)
            if (clean !== text) text = clean
        }
        onAccepted: boxes.accepted()
        Accessible.name: qsTr("Access code")
    }

    Row {
        spacing: 8
        Repeater {
            model: boxes.length
            Rectangle {
                required property int index
                readonly property bool current: input.activeFocus
                    && index === Math.min(input.text.length, boxes.length - 1)
                width: 48
                height: 54
                radius: Theme.radiusMd
                color: Theme.surface2
                border.width: current ? 2 : 1
                border.color: current ? Theme.accent : Theme.border
                Behavior on border.color { ColorAnimation { duration: Motion.fast } }
                Text {
                    anchors.centerIn: parent
                    text: input.text.charAt(index)
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
                // The caret, in the box where the next character goes.
                Rectangle {
                    visible: parent.current && input.text.length <= index && input.cursorVisible
                    anchors.centerIn: parent
                    width: 2
                    height: 22
                    color: Theme.accent
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.IBeamCursor
        onClicked: input.forceActiveFocus()
    }
}
