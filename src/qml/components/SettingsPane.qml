import QtQuick
import Scancore

// One Settings category: a heading and its groups. On desktop every pane is
// stacked on one scrolling page; on phones each opens on its own page, which
// shows the heading itself.
Column {
    id: pane
    property string title
    property string subtitle
    property string iconName
    property color tint: Theme.textMuted
    property bool showHeader: true
    default property alias groups: body.data

    width: parent ? parent.width : 700
    spacing: 18

    Row {
        visible: pane.showHeader
        spacing: 14
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 40
            radius: 12
            color: Theme.alpha(pane.tint, 0.14)
            Icon {
                anchors.centerIn: parent
                name: pane.iconName
                size: 19
                color: pane.tint
            }
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: pane.title
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textXl + 2
                font.weight: Font.DemiBold
                font.letterSpacing: -0.3
            }
            Text {
                visible: text.length > 0
                text: pane.subtitle
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
            }
        }
    }

    Column {
        id: body
        width: parent.width
        spacing: 22
    }
}
