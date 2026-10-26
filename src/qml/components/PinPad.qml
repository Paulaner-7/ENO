import QtQuick
import QtQuick.Controls.Basic
import Scancore

// A PIN entry: dots for the digits typed and a number pad (keys work too).
// `length` > 0 tries the PIN as soon as that many digits are in; 0 takes 4
// to 8 and waits for the tick. `extraIcon` adds a key left of 0 (the
// fingerprint on the lock screen).
Column {
    id: pad
    property int length: 0
    property string pin: ""
    property bool busy: false
    property string extraIcon: ""
    property string extraTip: ""
    signal entered(string pin)
    signal extra()

    function clear() { pin = "" }
    function shake() { shakeAnimation.restart(); pin = "" }
    function press(digit) {
        if (busy || pin.length >= (length > 0 ? length : 8)) return
        pin += digit
        if (length > 0 && pin.length === length) entered(pin)
    }
    function back() { if (!busy) pin = pin.slice(0, -1) }
    function submit() { if (!busy && pin.length >= 4 && length === 0) entered(pin) }

    spacing: 28
    focus: true
    Keys.onPressed: (event) => {
        if (event.text.length === 1 && event.text >= "0" && event.text <= "9") { press(event.text); event.accepted = true }
        else if (event.key === Qt.Key_Backspace) { back(); event.accepted = true }
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { submit(); event.accepted = true }
    }

    Row {
        id: dots
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14
        height: 14
        transform: Translate { id: nudge }
        Repeater {
            model: pad.length > 0 ? pad.length : Math.max(4, pad.pin.length)
            Rectangle {
                required property int index
                width: 14; height: 14; radius: 7
                color: index < pad.pin.length ? Theme.text : "transparent"
                border.width: 1.5
                border.color: index < pad.pin.length ? Theme.text : Theme.alpha(Theme.text, 0.35)
                scale: index === pad.pin.length - 1 ? 1.12 : 1
                Behavior on scale { NumberAnimation { duration: Motion.fast } }
            }
        }
        SequentialAnimation {
            id: shakeAnimation
            loops: 1
            NumberAnimation { target: nudge; property: "x"; to: -12; duration: 50 }
            NumberAnimation { target: nudge; property: "x"; to: 12; duration: 70 }
            NumberAnimation { target: nudge; property: "x"; to: -8; duration: 60 }
            NumberAnimation { target: nudge; property: "x"; to: 0; duration: 50 }
        }
    }

    Grid {
        anchors.horizontalCenter: parent.horizontalCenter
        columns: 3
        columnSpacing: 22
        rowSpacing: 14
        opacity: pad.busy ? 0.5 : 1
        Repeater {
            model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "extra", "0", "back"]
            AbstractButton {
                id: key
                required property string modelData
                readonly property bool digit: modelData !== "extra" && modelData !== "back"
                // Hidden by opacity: a Grid closes up around invisible items.
                readonly property bool shown: modelData !== "extra" || pad.extraIcon.length > 0 || pad.length === 0
                width: 68; height: 68
                enabled: shown && !pad.busy
                opacity: shown ? 1 : 0
                focusPolicy: Qt.NoFocus
                Accessible.name: modelData === "back" ? qsTr("Delete") : modelData === "extra" ? (pad.extraIcon ? pad.extraTip : qsTr("Done")) : modelData
                onClicked: {
                    if (digit) pad.press(modelData)
                    else if (modelData === "back") pad.back()
                    else if (pad.extraIcon) pad.extra()
                    else pad.submit()
                }
                background: Rectangle {
                    radius: width / 2
                    color: key.pressed ? Theme.alpha(Theme.text, 0.16) : key.digit ? Theme.alpha(Theme.text, 0.06)
                         : key.hovered ? Theme.alpha(Theme.text, 0.06) : "transparent"
                    Behavior on color { ColorAnimation { duration: Motion.fast } }
                }
                contentItem: Item {
                    Text {
                        anchors.centerIn: parent
                        visible: key.digit
                        text: key.modelData
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: 26
                        font.weight: Font.Medium
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: !key.digit
                        name: key.modelData === "back" ? "delete" : (pad.extraIcon || "check")
                        size: 24
                        color: key.modelData === "extra" && !pad.extraIcon && pad.pin.length < 4 ? Theme.textFaint : Theme.text
                    }
                }
            }
        }
    }
}
