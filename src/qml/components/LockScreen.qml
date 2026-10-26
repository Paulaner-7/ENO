import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Covers the window while the app lock is on (see Lock). On Android the
// fingerprint or face prompt comes up by itself when allowed. A modal popup
// above every other, so nothing behind it (dialogs, shortcuts, Back) can be
// reached.
Popup {
    id: screen
    readonly property bool biometric: Lock.biometricAvailable && App.settings.lockBiometric !== false
    property var task: null

    function askBiometric() { if (biometric && Lock.locked) Lock.useBiometric() }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 400
    height: parent ? parent.height : 800
    z: 1000
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    visible: Lock.locked
    Overlay.modal: Item {}
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast } }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic } }
    background: Rectangle { color: Theme.base }

    onOpened: { pad.clear(); pad.forceActiveFocus(); prompt.restart() }
    Timer { id: prompt; interval: 350; onTriggered: screen.askBiometric() }
    Connections {
        target: Qt.application
        function onStateChanged() { if (Qt.application.state === Qt.ApplicationActive && screen.visible) prompt.restart() }
    }
    Connections {
        target: screen.task
        function onFinished(ok) {
            if (!ok) pad.shake()
            screen.task = null
        }
    }

    contentItem: Item {
        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -20
            spacing: 36
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 14
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 56; height: 56; radius: 28
                    color: Theme.surface2
                    Icon { anchors.centerIn: parent; name: "lock"; size: 24; color: Theme.text }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("SCANCORE is locked")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textXl
                    font.weight: Font.DemiBold
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Lock.wait > 0 ? qsTr("Too many tries. Try again in %1 s.").arg(Lock.wait)
                        : Lock.failures > 0 ? qsTr("Wrong PIN") : qsTr("Enter your PIN")
                    color: Lock.failures > 0 ? Theme.danger : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                }
            }
            PinPad {
                id: pad
                anchors.horizontalCenter: parent.horizontalCenter
                length: Lock.length
                busy: screen.task !== null || Lock.wait > 0
                extraIcon: screen.biometric ? "fingerprint" : ""
                extraTip: qsTr("Use fingerprint or face")
                onEntered: (pin) => screen.task = Lock.unlock(pin)
                onExtra: Lock.useBiometric()
            }
        }
    }
}
