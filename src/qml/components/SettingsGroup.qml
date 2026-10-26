import QtQuick
import Scancore

// A card of SettingRows with an optional small heading above and a note
// below.
Column {
    id: group
    property string title
    property string note
    default property alias rows: body.data

    width: parent ? parent.width : 600
    spacing: 8

    Text {
        visible: group.title.length > 0
        leftPadding: 4
        text: group.title
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1.1
    }

    Rectangle {
        width: parent.width
        height: body.height
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        Column {
            id: body
            width: parent.width
        }
    }

    Text {
        visible: group.note.length > 0
        width: parent.width
        leftPadding: 4
        rightPadding: 4
        text: group.note
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs + 1
        wrapMode: Text.Wrap
    }
}
