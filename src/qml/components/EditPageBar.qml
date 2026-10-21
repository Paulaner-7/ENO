import QtQuick
import ScanCore

// The bar along the bottom of a page in edit mode: what to do, Reset and
// Done. `arrangers` are the page's PageArrangers (Reset clears them all).
Rectangle {
    id: bar
    property var arrangers: []
    property bool editing: false
    // Shows "Add section" (pages whose sections can be removed and added).
    property bool canAdd: false
    signal done()
    signal add()

    visible: editing
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: Theme.compact ? 84 : 28
    width: Math.min(parent.width - 2 * Theme.gutter, row.implicitWidth + 32)
    height: 56
    radius: height / 2
    color: Theme.surface
    border.width: 1
    border.color: Theme.borderStrong
    z: 50

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 12
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: !Theme.compact
            text: Theme.touch ? qsTr("Move sections with the arrows, hide them with the eye")
                              : qsTr("Drag sections to move them, hide them with the eye")
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
        }
        ScButton {
            visible: bar.canAdd
            text: qsTr("Add section")
            iconName: "plus"
            variant: "ghost"
            compact: true
            onClicked: bar.add()
        }
        ScButton {
            text: qsTr("Reset")
            iconName: "rotate-ccw"
            variant: "ghost"
            compact: true
            onClicked: bar.arrangers.forEach(a => a.reset())
        }
        ScButton {
            text: qsTr("Done")
            iconName: "check"
            variant: "primary"
            compact: true
            onClicked: bar.done()
        }
    }
}
