import QtQuick
import Scancore

// End-of-chapter card in the comic reader.
Column {
    id: card
    property var host
    spacing: 14
    width: 360

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: card.host.hasNext ? qsTr("End of ") + (card.host.unit.title || "chapter") : qsTr("You're all caught up")
        color: "#e4e4e7"
        font.family: Theme.fontDisplay
        font.pixelSize: Theme.textXl
        font.weight: Font.DemiBold
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: card.host.hasNext
        text: qsTr("Next: ") + (card.host.hasNext ? card.host.chapters[card.host.index + 1].title : "")
        color: "#9a9aa2"
        font.family: Theme.fontSans
        font.pixelSize: Theme.textSm
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10
        ScButton {
            visible: card.host.hasNext
            variant: "primary"
            text: qsTr("Next chapter")
            iconName: "skip-forward"
            onClicked: card.host.load(card.host.index + 1)
        }
        ScButton {
            text: qsTr("Back to details")
            onClicked: card.host.exit()
        }
    }
}
