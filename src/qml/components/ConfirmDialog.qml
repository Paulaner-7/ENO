import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// A short question with Cancel and a confirming button, or (asksText) a
// name to type. Springs in from the middle of the window over a dimmed page.
Dialog {
    id: dialog
    property string heading
    property string message
    property string confirmText: "OK"
    property bool destructive: false
    // A text field (a list's name, say): its text is `value`.
    property bool asksText: false
    property alias value: field.text
    property string placeholder
    signal confirmed()

    function confirm() {
        if (asksText && !field.text.trim()) return
        dialog.close()
        dialog.confirmed()
    }
    onOpened: if (asksText) { field.forceActiveFocus(); field.selectAll() }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(420, (parent ? parent.width : 420) - 32)
    modal: true
    focus: true
    padding: 24
    title: ""

    Overlay.modal: Rectangle { color: Theme.overlay }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast }
            NumberAnimation { property: "scale"; to: 0.97; duration: Motion.fast }
        }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
    }

    contentItem: Column {
        spacing: 16
        Text {
            width: parent.width
            text: dialog.heading
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textLg
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: dialog.message
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            lineHeight: 1.25
            wrapMode: Text.Wrap
        }
        InlineField {
            id: field
            visible: dialog.asksText
            width: parent.width
            placeholderText: dialog.placeholder
            maximumLength: 60
            onAccepted: dialog.confirm()
        }
        Row {
            anchors.right: parent.right
            spacing: 8
            ScButton {
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }
            ScButton {
                text: dialog.confirmText
                variant: dialog.destructive ? "danger" : "primary"
                enabled: !dialog.asksText || field.text.trim().length > 0
                onClicked: dialog.confirm()
            }
        }
    }
}
