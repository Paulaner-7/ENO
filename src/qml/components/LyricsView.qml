import QtQuick
import QtQuick.Controls.Basic
import Scancore

// A song's lyrics, following the song: the line being sung is lit and kept
// in the middle, the ones before dimmed. Tap a line to go there; scroll
// freely and the view comes back to the song a few seconds later. Plain
// lyrics (no timing) just scroll. The current line is found by halving the
// list, not by looking at every line on every position change.
Item {
    id: view
    property var song: null
    property real position: AudioPlayer.position
    property real duration: AudioPlayer.duration
    // Bigger type for the full-screen lyrics.
    property bool large: false
    property color ink: Theme.text
    signal seekRequested(real seconds)

    property var task: null
    readonly property var result: task && task.ok ? task.result : null
    readonly property var lines: result && result.synced ? result.synced : []
    readonly property bool synced: lines.length > 0
    readonly property string plain: result && result.plain ? result.plain : ""

    // Asked again for each song (and once its length is known, which picks
    // the right version).
    function load() {
        if (!song) { task = null; return }
        task = Music.lyrics(song.title || "", (song.artists || [])[0] || "", song.album || "", duration > 0 ? duration : 0, song.file || "", song.lrc || "")
    }
    onSongChanged: load()
    property bool askedWithLength: false
    onDurationChanged: if (song && duration > 0 && !askedWithLength && !(result && result.found)) { askedWithLength = true; load() }
    Component.onCompleted: load()

    // The line at `position`: the last one starting at or before it.
    readonly property int current: {
        const list = lines
        if (!list.length || position < list[0].time) return -1
        let lo = 0, hi = list.length - 1
        while (lo < hi) {
            const mid = (lo + hi + 1) >> 1
            if (list[mid].time <= position + 0.15) lo = mid
            else hi = mid - 1
        }
        return lo
    }

    // The reader's own scrolling holds the view still for a while; then the
    // list centres the line being sung again by itself (its highlight range).
    property bool following: true
    Timer { id: comeBack; interval: 3500; onTriggered: view.following = true }

    ListView {
        id: list
        visible: view.synced
        anchors.fill: parent
        model: view.lines
        clip: true
        spacing: view.large ? 18 : 12
        currentIndex: view.current
        // The line being sung stays in the middle band.
        highlightRangeMode: view.following ? ListView.ApplyRange : ListView.NoHighlightRange
        preferredHighlightBegin: height * 0.36
        preferredHighlightEnd: height * 0.5
        highlightMoveDuration: Motion.reduced ? 0 : 420
        highlightMoveVelocity: -1
        highlightFollowsCurrentItem: true
        highlight: Item {}
        topMargin: height * 0.36
        bottomMargin: height * 0.5
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: Motion.flickVelocity
        flickDeceleration: Motion.flickDeceleration
        cacheBuffer: 600
        WheelScroll {}
        onMovementStarted: { view.following = false; comeBack.restart() }
        onMovementEnded: comeBack.restart()
        delegate: Text {
            id: line
            required property var modelData
            required property int index
            readonly property bool now: index === view.current
            readonly property bool past: index < view.current
            width: list.width
            text: modelData.text.length ? modelData.text : "♪"
            color: view.ink
            opacity: now ? 1 : past ? 0.38 : 0.55
            scale: now && !Motion.reduced ? 1.0 : 0.96
            transformOrigin: Item.Left
            font.family: Theme.fontDisplay
            font.pixelSize: view.large ? (Theme.compact ? 26 : 34) : (Theme.compact ? 19 : 22)
            font.weight: now ? Font.Bold : Font.DemiBold
            wrapMode: Text.Wrap
            lineHeight: 1.15
            Behavior on opacity { NumberAnimation { duration: Motion.normal } }
            Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }
            Accessible.role: Accessible.Button
            Accessible.name: text
            TapHandler {
                onTapped: {
                    view.seekRequested(line.modelData.time)
                    view.following = true
                }
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    // No timing: the words, to scroll through.
    Flickable {
        visible: !view.synced && view.plain.length > 0
        anchors.fill: parent
        contentHeight: plainText.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        WheelScroll {}
        Text {
            id: plainText
            width: parent.width
            text: view.plain
            color: view.ink
            opacity: 0.85
            font.family: Theme.fontSans
            font.pixelSize: view.large ? Theme.textXl : Theme.textLg
            wrapMode: Text.Wrap
            lineHeight: 1.4
        }
    }

    // Looking, none, or an instrumental.
    Column {
        visible: !view.synced && view.plain.length === 0
        anchors.centerIn: parent
        width: parent.width - 32
        spacing: 10
        Spinner {
            visible: !!view.task && view.task.loading
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            visible: !(view.task && view.task.loading)
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: view.result && view.result.instrumental ? qsTr("An instrumental: no words to sing along to.")
                : view.task && view.task.failed ? qsTr("Lyrics could not be loaded. Check the connection.")
                : qsTr("No lyrics for this song yet.")
            color: view.ink
            opacity: 0.7
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
            wrapMode: Text.Wrap
        }
        Text {
            visible: !(view.task && view.task.loading)
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Lyrics come from LRCLIB, written by its community.")
            color: view.ink
            opacity: 0.45
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            wrapMode: Text.Wrap
        }
    }
}
