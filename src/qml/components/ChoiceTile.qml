import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// A selectable card with a small picture above its label, for choices that
// are easier to see than to read (theme, reader layout).
AbstractButton {
    id: tile
    property bool selected: false
    property string label
    property string detail
    default property alias picture: art.data

    implicitWidth: 168
    implicitHeight: 92 + 54
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: tile.selected ? 2 : 1
        border.color: tile.selected ? Theme.accent : tile.hovered ? Theme.borderStrong : Theme.border
        Behavior on border.color { ColorAnimation { duration: Motion.fast } }
    }

    contentItem: Item {
        Rectangle {
            id: frame
            x: 8
            y: 8
            width: parent.width - 16
            height: 84
            radius: Theme.radiusMd
            color: Theme.surface2
            clip: true
            Item {
                id: art
                anchors.fill: parent
            }
        }
        Row {
            x: 12
            anchors.top: frame.bottom
            anchors.topMargin: 10
            spacing: 8
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                radius: 8
                color: tile.selected ? Theme.accent : "transparent"
                border.width: tile.selected ? 0 : 1.5
                border.color: Theme.borderStrong
                Icon {
                    anchors.centerIn: parent
                    visible: tile.selected
                    name: "check"
                    size: 11
                    color: Theme.accentInk
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: tile.width - 48
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    text: tile.label
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    visible: text.length > 0
                    text: tile.detail
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs
                }
            }
        }
    }

    scale: pressed ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: Motion.fast } }
}
