import QtQuick
import Scancore

// The bar along the bottom of a page in edit mode: what to do, Reset and
// Done. `arrangers` are the page's PageArrangers (Reset clears them all).
Item {
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
    z: 50

    LiquidGlass {
        anchors.fill: parent
        zTier: 1
        radius: height / 2
        interactive: true
        trackPointer: true
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 12
        Item {
            visible: !Theme.compact
            width: hintText.implicitWidth
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            NativeGlassButton {
                anchors.fill: parent
                visible: !Theme.eink && isSupported
                glass: false
                zTier: 1
                passthrough: true
                text: hintText.text
                textColor: Theme.textMuted
                fontSize: Theme.textSm
            }
            Text {
                id: hintText
                anchors.centerIn: parent
                text: Theme.touch ? qsTr("Move sections with the arrows, hide them with the eye")
                                  : qsTr("Drag sections to move them, hide them with the eye")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
            }
        }
        ScButton {
            visible: bar.canAdd
            text: qsTr("Add section")
            iconName: "plus"
            variant: "ghost"
            compact: true
            inGlassBar: true
            zTier: 1
            onClicked: bar.add()
        }
        ScButton {
            text: qsTr("Reset")
            iconName: "rotate-ccw"
            variant: "ghost"
            compact: true
            inGlassBar: true
            zTier: 1
            onClicked: bar.arrangers.forEach(a => a.reset())
        }
        ScButton {
            text: qsTr("Done")
            iconName: "check"
            variant: "primary"
            compact: true
            inGlassBar: true
            zTier: 1
            onClicked: bar.done()
        }
    }
}
