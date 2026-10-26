import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Reading aloud with the reader closed (audiobook mode): the book, the
// chapter, play or pause and stop, over the bottom of the pages. Tapping it
// opens the reader where the voice is.
AbstractButton {
    id: bar
    readonly property bool shown: ReadAloud.active
    visible: shown
    height: shown ? 60 : 0
    hoverEnabled: true
    Accessible.name: qsTr("Reading aloud: %1").arg(ReadAloud.series.title || "")
    onClicked: Router.openNovelReader(ReadAloud.series, ReadAloud.chapters, ReadAloud.chapterIndex)

    background: Rectangle {
        color: bar.hovered ? Theme.surface2 : Theme.surface
        border.width: 1
        border.color: Theme.border
        // How far through the chapter.
        Rectangle {
            width: parent.width * ReadAloud.progress
            height: 2
            color: Theme.accent
            Behavior on width { NumberAnimation { duration: Motion.normal } }
        }
    }

    contentItem: Item {
        Cover {
            id: cover
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 44
            radius: Theme.radiusSm
            source: ReadAloud.series.cover || ""
            referer: ReadAloud.series.sourceUrl ? Novels.imageReferer(ReadAloud.series.sourceUrl) : ""
            title: ReadAloud.series.title || ""
        }
        Column {
            anchors.left: cover.right
            anchors.leftMargin: 12
            anchors.right: buttons.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                width: parent.width
                text: ReadAloud.chapterTitle || ReadAloud.series.title || ""
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: (ReadAloud.loading ? qsTr("Loading the chapter…") : ReadAloud.series.title || "")
                    + (ReadAloud.sleepMode === "minutes" ? " · " + qsTr("stops at %1").arg(Qt.formatTime(new Date(ReadAloud.sleepEndsAt), "hh:mm"))
                       : ReadAloud.sleepMode === "chapter" ? " · " + qsTr("stops after this chapter") : "")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                elide: Text.ElideRight
            }
        }
        Row {
            id: buttons
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            IconButton {
                iconName: "skip-back"
                tip: qsTr("Previous sentence")
                visible: !Theme.compact
                onClicked: ReadAloud.skip(-1)
            }
            IconButton {
                iconName: ReadAloud.playing ? "pause" : "play"
                tip: ReadAloud.playing ? qsTr("Pause") : qsTr("Go on reading aloud")
                onClicked: ReadAloud.toggle()
            }
            IconButton {
                iconName: "skip-forward"
                tip: qsTr("Next sentence")
                onClicked: ReadAloud.skip(1)
            }
            IconButton {
                iconName: "x"
                tip: qsTr("Stop reading aloud")
                onClicked: ReadAloud.stop()
            }
        }
    }
}
