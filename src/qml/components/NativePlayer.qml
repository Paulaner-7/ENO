import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Plays a video from an anime extension with SCANCORE's own player (libmpv,
// MpvPlayer), with controls along the bottom: play, seek, time, subtitles,
// audio, speed and volume. It shows the same face to WatchPage as the web
// players (url, seek(), time(), interacted(), key()).
Item {
    id: root
    property string url
    // The video as Anime.videos() gives it: headers, subtitles, audio,
    // mpvArgs.
    property var video: ({})
    property real startAt: 0
    // WatchPage's chrome: the controls show and hide with it.
    property bool chromeShown: true
    readonly property bool phone: App.platform === "android"
    readonly property alias player: mpv
    readonly property bool controlsShown: chromeShown || mpv.paused || mpv.buffering || seekBar.pressed || menu.visible

    signal time(real seconds, real total, string status)
    signal interacted()
    signal key(var press)
    signal unsupported()
    signal failed(string message)
    signal fullScreenRequested()

    function seek(seconds) { mpv.seek(seconds) }
    function togglePause() { mpv.togglePause(); interacted() }
    function seekBy(seconds) { mpv.seekBy(seconds); interacted() }
    function changeVolume(delta) {
        const v = Math.max(0, Math.min(130, Math.round(mpv.volume + delta)))
        mpv.volume = v
        App.setSetting("playerVolume", v)
        interacted()
    }

    function keyHint(action) {
        const keys = Keymap.labels(action)
        return keys.length ? " (" + keys[0] + ")" : ""
    }

    function formatTime(s) {
        s = Math.max(0, Math.floor(s || 0))
        const h = Math.floor(s / 3600)
        const m = Math.floor(s / 60) % 60
        const sec = String(s % 60).padStart(2, "0")
        return h > 0 ? h + ":" + String(m).padStart(2, "0") + ":" + sec : m + ":" + sec
    }

    MpvPlayer {
        id: mpv
        anchors.fill: parent
        source: root.url
        headers: root.video.headers || ({})
        options: root.video.mpvArgs || ({})
        subtitles: root.video.subtitles || []
        look: ({ subtitleScale: Number(App.settings.subtitleScale) || 1, subtitleOutline: Number(App.settings.subtitleOutline),
                 subtitleColor: App.settings.subtitleColor || "#ffffff", subtitleBox: App.settings.subtitleBox === true,
                 sharpen: App.settings.animeSharpen || "off" })
        audio: root.video.audio || []
        startAt: root.startAt
        // Above the controls while they show.
        subtitlePosition: root.controlsShown && bar.visible ? Math.round(100 - (bar.height + 6) / Math.max(1, root.height) * 100) : 100
        Component.onCompleted: {
            volume = Number(App.settings.playerVolume ?? 100)
            speed = Number(App.settings.playerSpeed || 1)
        }
        onPositionChanged: root.time(position, duration, ended ? "Ended" : paused ? "Paused" : "Playing")
        onFinished: root.time(duration, duration, "Ended")
        onErrorChanged: if (error) root.failed(error)
    }

    // Desktop: a click pauses, a double click goes full screen. Phone: a
    // tap shows the controls, a double tap on either side skips 10 s.
    MouseArea {
        anchors.fill: parent
        anchors.bottomMargin: bar.visible ? bar.height : 0
        acceptedButtons: Qt.LeftButton
        Timer { id: clickDelay; interval: 220; onTriggered: root.phone ? root.interacted() : root.togglePause() }
        onClicked: clickDelay.restart()
        onDoubleClicked: (mouse) => {
            clickDelay.stop()
            if (!root.phone) root.fullScreenRequested()
            else root.seekBy(mouse.x < width / 2 ? -10 : 10)
        }
    }

    Spinner {
        anchors.centerIn: parent
        visible: mpv.buffering && !mpv.error
        size: 34
        color: "#e4e4e7"
    }

    // A tap target in the middle while paused (phones above all).
    IconButton {
        anchors.centerIn: parent
        visible: mpv.paused && !mpv.buffering && mpv.loaded && !mpv.ended
        diameter: 64
        iconSize: 30
        iconName: "play"
        iconColor: "white"
        background: Rectangle { radius: 32; color: Qt.rgba(0, 0, 0, 0.55) }
        onClicked: root.togglePause()
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 48)
        spacing: 10
        visible: mpv.error.length > 0 || !mpv.available
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "tv-minimal"
            size: 30
            color: "#a1a1aa"
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: !mpv.available ? qsTr("This copy of SCANCORE has no video player for anime from extensions.") : mpv.error
            color: "#d4d4d8"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
        }
    }

    // ------------------------------------------------------------ controls
    Rectangle {
        id: bar
        anchors.bottom: parent.bottom
        width: parent.width
        height: root.phone ? 64 : 76
        visible: opacity > 0.01 && mpv.available
        opacity: root.controlsShown ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Motion.slow } }
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.85) }
        }
        HoverHandler { id: barHover }

        // Seek bar with what is downloaded already.
        Slider {
            id: seekBar
            x: 12
            width: parent.width - 24
            y: root.phone ? 6 : 12
            height: 22
            from: 0
            to: Math.max(1, mpv.duration)
            hoverEnabled: true
            // Follows the video unless it is being dragged.
            Binding on value { when: !seekBar.pressed; value: mpv.position }
            onPressedChanged: if (!pressed) mpv.seek(value)
            background: Rectangle {
                x: seekBar.leftPadding
                y: seekBar.topPadding + seekBar.availableHeight / 2 - height / 2
                width: seekBar.availableWidth
                height: seekBar.hovered || seekBar.pressed ? 6 : 4
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.18)
                Rectangle {
                    width: Math.min(1, (mpv.position + mpv.buffered) / Math.max(1, mpv.duration)) * parent.width
                    height: parent.height
                    radius: height / 2
                    color: Qt.rgba(1, 1, 1, 0.28)
                }
                Rectangle {
                    width: seekBar.visualPosition * parent.width
                    height: parent.height
                    radius: height / 2
                    color: Theme.accent
                }
            }
            handle: Rectangle {
                x: seekBar.leftPadding + seekBar.visualPosition * (seekBar.availableWidth - width)
                y: seekBar.topPadding + seekBar.availableHeight / 2 - height / 2
                width: seekBar.pressed || seekBar.hovered ? 14 : 0
                height: width
                radius: width / 2
                color: "white"
                Behavior on width { NumberAnimation { duration: Motion.fast } }
            }
            // The time under the pointer.
            Rectangle {
                visible: seekBar.hovered && !root.phone && mpv.duration > 0
                readonly property real at: hoverPoint.point.position.x / seekBar.width
                x: Math.max(0, Math.min(seekBar.width - width, hoverPoint.point.position.x - width / 2))
                y: -height - 4
                width: hoverTime.implicitWidth + 12
                height: 22
                radius: 6
                color: "#18181b"
                Text {
                    id: hoverTime
                    anchors.centerIn: parent
                    text: root.formatTime(parent.at * mpv.duration)
                    color: "white"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs + 1
                }
            }
            HoverHandler { id: hoverPoint }
        }

        Row {
            x: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.phone ? 4 : 8
            spacing: 2
            IconButton {
                iconName: mpv.paused || mpv.ended ? "play" : "pause"
                iconColor: "white"
                tip: (mpv.paused ? qsTr("Play") : qsTr("Pause")) + root.keyHint("player.pause")
                onClicked: mpv.ended ? mpv.seek(0) : root.togglePause()
            }
            IconButton {
                visible: !root.phone
                iconName: mpv.muted || mpv.volume === 0 ? "volume-x" : "volume-2"
                iconColor: "white"
                tip: (mpv.muted ? qsTr("Unmute") : qsTr("Mute")) + root.keyHint("player.mute")
                onClicked: mpv.muted = !mpv.muted
            }
            Slider {
                id: volumeBar
                visible: !root.phone
                width: 90
                height: 34
                from: 0
                to: 100
                Binding on value { when: !volumeBar.pressed; value: mpv.muted ? 0 : Math.min(100, mpv.volume) }
                onMoved: {
                    mpv.muted = false
                    mpv.volume = value
                }
                onPressedChanged: if (!pressed) App.setSetting("playerVolume", Math.round(value))
                background: Rectangle {
                    x: volumeBar.leftPadding
                    y: volumeBar.topPadding + volumeBar.availableHeight / 2 - 2
                    width: volumeBar.availableWidth
                    height: 4
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.2)
                    Rectangle { width: volumeBar.visualPosition * parent.width; height: 4; radius: 2; color: "white" }
                }
                handle: Rectangle {
                    x: volumeBar.leftPadding + volumeBar.visualPosition * (volumeBar.availableWidth - width)
                    y: volumeBar.topPadding + volumeBar.availableHeight / 2 - height / 2
                    width: 12
                    height: 12
                    radius: 6
                    color: "white"
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 8
                text: root.formatTime(seekBar.pressed ? seekBar.value : mpv.position) + " / " + root.formatTime(mpv.duration)
                color: "#e4e4e7"
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                font.features: { "tnum": 1 }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.phone ? 4 : 8
            spacing: 2
            IconButton {
                id: subsButton
                readonly property var subs: mpv.tracks.filter(t => t.type === "sub")
                visible: subs.length > 0
                iconName: "captions"
                iconColor: subs.some(t => t.selected) ? "white" : "#a1a1aa"
                tip: qsTr("Subtitles")
                onClicked: menu.show(subsButton, [{ label: qsTr("Off"), selected: !subs.some(t => t.selected), pick: () => mpv.setTrack("sub", 0) }]
                    .concat(subs.map(t => ({ label: t.title || t.lang || qsTr("Track ") + t.id, selected: t.selected, pick: () => mpv.setTrack("sub", t.id) }))))
            }
            IconButton {
                id: audioButton
                readonly property var tracks: mpv.tracks.filter(t => t.type === "audio")
                visible: tracks.length > 1
                iconName: "languages"
                iconColor: "white"
                tip: qsTr("Audio")
                onClicked: menu.show(audioButton, tracks.map(t => ({ label: t.title || t.lang || qsTr("Track ") + t.id, selected: t.selected, pick: () => mpv.setTrack("audio", t.id) })))
            }
            IconButton {
                id: speedButton
                iconName: "gauge"
                iconColor: mpv.speed !== 1 ? "white" : "#a1a1aa"
                tip: qsTr("Speed")
                onClicked: menu.show(speedButton, [0.5, 0.75, 1, 1.25, 1.5, 2].map(s => ({
                    label: s === 1 ? qsTr("Normal") : s + "×", selected: Math.abs(mpv.speed - s) < 0.01,
                    pick: () => { mpv.speed = s; App.setSetting("playerSpeed", s) } })))
            }
            // Phones: landscape is full screen (WatchPage turns the phone).
            IconButton {
                readonly property bool full: root.phone ? root.width > root.height
                                                        : root.Window.window !== null && root.Window.window.visibility === Window.FullScreen
                iconName: full ? "minimize" : "maximize"
                iconColor: "white"
                tip: full ? qsTr("Exit full screen") : qsTr("Full screen")
                onClicked: { root.fullScreenRequested(); root.interacted() }
            }
        }
    }

    // A list of choices above a control button.
    Popup {
        id: menu
        property var items: []
        function show(button, list) {
            items = list
            const p = button.mapToItem(root, 0, 0)
            x = Math.max(8, Math.min(root.width - width - 8, p.x + button.width / 2 - width / 2))
            y = p.y - height - 8
            open()
        }
        width: 200
        height: Math.min(column.implicitHeight + 12, root.height - 100)
        padding: 6
        background: Rectangle { radius: Theme.radiusMd; color: "#141416"; border.width: 1; border.color: "#2a2a30" }
        contentItem: Flickable {
            contentHeight: column.implicitHeight
            clip: true
            Column {
                id: column
                width: parent.width
                Repeater {
                    model: menu.items
                    AbstractButton {
                        id: choice
                        required property var modelData
                        width: column.width
                        height: 34
                        hoverEnabled: true
                        onClicked: {
                            modelData.pick()
                            menu.close()
                        }
                        background: Rectangle { radius: Theme.radiusSm; color: choice.hovered ? "#222226" : "transparent" }
                        contentItem: Row {
                            spacing: 8
                            leftPadding: 8
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "check"
                                size: 14
                                color: "white"
                                opacity: choice.modelData.selected ? 1 : 0
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: choice.width - 40
                                text: choice.modelData.label
                                color: "white"
                                elide: Text.ElideRight
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                            }
                        }
                    }
                }
            }
        }
    }

    // The keys from Settings → Keyboard (player).
    Shortcut { sequences: Keymap.sequences("player.pause"); enabled: root.visible && mpv.available; onActivated: root.togglePause() }
    Shortcut { sequences: Keymap.sequences("player.forward"); enabled: root.visible && mpv.available; onActivated: root.seekBy(10) }
    Shortcut { sequences: Keymap.sequences("player.backward"); enabled: root.visible && mpv.available; onActivated: root.seekBy(-10) }
    Shortcut { sequences: Keymap.sequences("player.volumeUp"); enabled: root.visible && mpv.available; onActivated: root.changeVolume(5) }
    Shortcut { sequences: Keymap.sequences("player.volumeDown"); enabled: root.visible && mpv.available; onActivated: root.changeVolume(-5) }
    Shortcut { sequences: Keymap.sequences("player.mute"); enabled: root.visible && mpv.available; onActivated: mpv.muted = !mpv.muted }
}
