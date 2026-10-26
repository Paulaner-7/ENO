import QtQuick
import QtQuick.Controls.Basic
import Scancore

// The song playing, small, over every page: its cover, title and singer,
// play/pause and next, and a thin line of how far it is. Tapping it opens
// Now Playing; swiping it up does too.
AbstractButton {
    id: mini
    readonly property var song: MusicQueue.current
    implicitHeight: 60
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    Accessible.name: song ? qsTr("Now playing: %1").arg(song.title) : ""
    onClicked: Router.openNowPlaying()

    DragHandler {
        target: null
        xAxis.enabled: false
        onActiveChanged: if (!active && translation.y < -40) Router.openNowPlaying()
    }

    background: GlassPanel {
        radius: Theme.retro ? 0 : Theme.radiusLg
        fill: Theme.retro ? Theme.surface2 : Theme.surface
        border.width: Theme.retro ? 0 : 1
        border.color: Theme.borderStrong
        // How far into the song.
        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.retro ? 0 : 10
            anchors.bottomMargin: 0
            height: 2
            width: (parent.width - (Theme.retro ? 0 : 20)) * (AudioPlayer.duration > 0 ? AudioPlayer.position / AudioPlayer.duration : 0)
            color: Theme.retro ? Theme.tile : Theme.accent
        }
    }
    contentItem: Item {
        Cover {
            id: art
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            radius: Theme.retro ? 0 : Theme.radiusSm
            source: mini.song ? mini.song.cover || "" : ""
            title: mini.song ? mini.song.anime || mini.song.album || mini.song.title || "" : ""
        }
        Column {
            anchors.left: art.right
            anchors.leftMargin: 12
            anchors.right: buttons.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            Text {
                width: parent.width
                text: mini.song ? mini.song.title : ""
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: mini.song ? MusicQueue.artistsOf(mini.song) || mini.song.anime || mini.song.album || "" : ""
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                elide: Text.ElideRight
            }
        }
        Row {
            id: buttons
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            IconButton {
                iconName: AudioPlayer.loading ? "loader-circle" : AudioPlayer.paused ? "play" : "pause"
                iconSize: 20
                diameter: 42
                tip: AudioPlayer.paused ? qsTr("Play") : qsTr("Pause")
                onClicked: AudioPlayer.toggle()
            }
            IconButton {
                iconName: "skip-forward"
                diameter: 42
                enabled: MusicQueue.hasNext
                tip: qsTr("Next")
                onClicked: MusicQueue.next()
            }
            IconButton {
                visible: mini.width >= 360
                iconName: "x"
                diameter: 42
                tip: qsTr("Stop")
                onClicked: MusicQueue.clear()
            }
        }
    }
}
