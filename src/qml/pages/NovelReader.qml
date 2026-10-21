import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import ScanCore

// Full-window novel reader with adjustable typography, reading themes,
// remembered position and the next chapter loaded ahead.
Rectangle {
    id: reader
    property var series: ({})
    property var chapters: []
    property int index: 0

    readonly property var unit: chapters[index] || ({})
    readonly property bool hasPrev: index > 0
    readonly property bool hasNext: index < chapters.length - 1
    property var task: null
    property var nextTask: null
    property int nextFor: -1
    readonly property var blocks: task && task.ok ? task.result.blocks : []
    property bool barsShown: true
    property real lastY: 0
    property var resumePosition: null
    property bool finished: false

    readonly property var themes: Theme.readingThemes
    readonly property var look: themes[App.settings.novelTheme] || themes.app
    readonly property int fontSize: App.settings.novelSize || 19
    readonly property real lineHeight: App.settings.novelLineHeight || 1.5
    readonly property int columnWidth: Math.min(width - 64, App.settings.novelWidth || 720)
    readonly property string fontFamily: App.settings.novelFont || Theme.fontSerif

    // Drawn under the phone's status bar in the reading theme's colour; the
    // bars and text are padded by the inset instead.
    readonly property bool edgeToEdge: true
    readonly property real insetTop: SafeArea.margins.top
    readonly property real insetBottom: SafeArea.margins.bottom

    color: look.bg
    focus: true
    Behavior on color { ColorAnimation { duration: Motion.slow } }

    function load(i) {
        savePosition(true)
        index = i
        finished = false
        const p = Library.progressMap(series.id)[chapters[i].key]
        resumePosition = p && !p.completed && p.position ? p.position : null
        // The next chapter fetched ahead, finished or still on its way.
        const ahead = nextTask && nextFor === i && (nextTask.loading || nextTask.ok)
        task = ahead ? nextTask : Novels.chapter(chapters[i].url, series.id)
        nextTask = null
        nextFor = -1
        flick.scrollTo(0, false)
        barsShown = true
        if (task.done) Qt.callLater(arrived)
        contentReveal.restart()
        Presence.show("novel", series.title || "", chapters[i].title || "")
    }

    Component.onCompleted: {
        load(index)
        Activity.setMedia("novel")
    }
    Component.onDestruction: {
        Activity.setMedia("")
        Presence.clear()
        savePosition(true)
    }

    Connections {
        target: reader.task
        function onFinished() { Qt.callLater(reader.arrived) }
    }

    // A chapter's text is in: go back to where reading stopped, and count a
    // chapter that fits on one screen as read.
    function arrived() {
        if (blocks.length === 0) return
        // The column places its blocks later; their positions are needed now.
        body.forceLayout()
        if (resumePosition && resumePosition.block !== undefined) {
            const item = blockRepeater.itemAt(Math.min(resumePosition.block, blockRepeater.count - 1))
            if (item) flick.scrollTo(item.y + body.y - 80, false)
        }
        resumePosition = null
        if (flick.contentHeight - flick.height <= 4) {
            finished = true
            savePosition(true)
        }
    }

    function firstVisibleBlock() {
        const y = flick.contentY - body.y + 80
        let lo = 0, hi = blockRepeater.count - 1, found = 0
        while (lo <= hi) {
            const mid = (lo + hi) >> 1
            const it = blockRepeater.itemAt(mid)
            if (!it) break
            if (it.y <= y) { found = mid; lo = mid + 1 } else hi = mid - 1
        }
        return found
    }

    Timer {
        id: saveTimer
        interval: 600
        onTriggered: reader.commitPosition()
    }
    function commitPosition() {
        if (!series.id || !unit.key || blocks.length === 0) return
        const max = Math.max(1, flick.contentHeight - flick.height)
        const percent = finished ? 1 : Math.min(1, Math.max(0, flick.contentY / max))
        Library.recordProgress(series, unit, { type: "text", block: firstVisibleBlock(), percent: percent }, percent, finished || percent > 0.97)
    }
    function savePosition(force) {
        if (!series.id || !unit.key || blocks.length === 0) return
        if (force) {
            saveTimer.stop()
            commitPosition()
        } else {
            saveTimer.restart()
        }
    }

    function exit() {
        savePosition(true)
        Router.back()
    }

    function keyHint(action) {
        const keys = Keymap.labels(action)
        return keys.length ? " (" + keys[0] + ")" : ""
    }

    function toggleFullScreen() {
        Window.window.visibility === Window.FullScreen ? Window.window.showNormal() : Window.window.showFullScreen()
    }

    // The keys come from Settings → Keyboard (see Keymap), as in the comic
    // reader; scrolling keys only while the reader has focus.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) exit()
        else if (Keymap.matches("reader.down", event)) flick.scrollTo(flick.target + 90)
        else if (Keymap.matches("reader.up", event)) flick.scrollTo(flick.target - 90)
        else if (Keymap.matches("reader.pageDown", event)) flick.scrollTo(flick.contentY + flick.height * 0.85)
        else if (Keymap.matches("reader.pageUp", event)) flick.scrollTo(flick.contentY - flick.height * 0.85)
        else if (event.key === Qt.Key_Right) { if (hasNext) load(index + 1) }
        else if (event.key === Qt.Key_Left) { if (hasPrev) load(index - 1) }
        else return
        event.accepted = true
    }
    Shortcut { sequences: Keymap.sequences("reader.next"); onActivated: if (reader.hasNext) reader.load(reader.index + 1) }
    Shortcut { sequences: Keymap.sequences("reader.prev"); onActivated: if (reader.hasPrev) reader.load(reader.index - 1) }
    Shortcut { sequences: Keymap.sequences("reader.first"); onActivated: flick.scrollTo(0) }
    Shortcut { sequences: Keymap.sequences("reader.last"); onActivated: flick.scrollTo(flick.contentHeight) }
    Shortcut { sequences: Keymap.sequences("reader.fullscreen"); onActivated: reader.toggleFullScreen() }
    Shortcut { sequences: Keymap.sequences("reader.wider"); onActivated: App.setSetting("novelSize", Math.min(34, reader.fontSize + 1)) }
    Shortcut { sequences: Keymap.sequences("reader.narrower"); onActivated: App.setSetting("novelSize", Math.max(13, reader.fontSize - 1)) }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: body.y + body.height + 120 + reader.insetBottom
        wheelStep: 110
        ScrollBar.vertical: ScScrollBar {}

        // Phone: a tap toggles the bar. On the flickable, which takes
        // touches before anything behind it.
        TapHandler {
            enabled: App.platform === "android"
            onSingleTapped: reader.barsShown = !reader.barsShown
        }

        onContentYChanged: {
            const delta = contentY - reader.lastY
            if (Math.abs(delta) > 6) {
                reader.barsShown = delta < 0 || contentY < 60
                reader.lastY = contentY
            }
            reader.savePosition(false)
            const max = contentHeight - height
            if (max > 0 && contentY / max > 0.6 && reader.hasNext && reader.nextFor !== reader.index + 1) {
                reader.nextFor = reader.index + 1
                reader.nextTask = Novels.chapter(reader.chapters[reader.index + 1].url, reader.series.id)
            }
            if (max > 0 && contentY >= max - 4 && reader.blocks.length > 0) {
                reader.finished = true
                reader.savePosition(true)
            }
        }

        Column {
            id: body
            x: (flick.width - width) / 2
            y: 110 + reader.insetTop
            width: reader.columnWidth
            spacing: reader.fontSize * 0.9

            opacity: 1
            transform: Translate { id: bodyShift; y: 0 }
            ParallelAnimation {
                id: contentReveal
                NumberAnimation { target: body; property: "opacity"; from: 0; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
                NumberAnimation { target: bodyShift; property: "y"; from: 18; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
            }

            Text {
                width: parent.width
                text: reader.series.title || ""
                color: reader.look.muted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1.3
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: (reader.task && reader.task.ok && reader.task.result.title) ? reader.task.result.title : (reader.unit.title || "")
                color: reader.look.text
                font.family: Theme.fontDisplay
                font.pixelSize: Math.round(reader.fontSize * 1.55)
                font.weight: Font.Bold
                wrapMode: Text.Wrap
                bottomPadding: reader.fontSize * 0.6
            }

            Repeater {
                id: blockRepeater
                model: reader.blocks
                delegate: Item {
                    required property var modelData
                    width: body.width
                    height: modelData.kind === "break" ? reader.fontSize * 1.6 : text.implicitHeight
                    Text {
                        id: text
                        visible: modelData.kind !== "break"
                        width: parent.width
                        leftPadding: modelData.kind === "quote" ? 22 : 0
                        text: modelData.html
                        textFormat: Text.StyledText
                        color: reader.look.text
                        font.family: modelData.kind === "heading" ? Theme.fontDisplay : reader.fontFamily
                        font.pixelSize: modelData.kind === "heading" ? Math.round(reader.fontSize * 1.2) : reader.fontSize
                        font.weight: modelData.kind === "heading" ? Font.DemiBold : Font.Normal
                        font.italic: modelData.kind === "quote"
                        lineHeight: reader.lineHeight
                        wrapMode: Text.Wrap
                        horizontalAlignment: App.settings.novelJustify === true ? Text.AlignJustify : Text.AlignLeft
                        Rectangle {
                            visible: modelData.kind === "quote"
                            width: 3
                            height: parent.height
                            radius: 2
                            color: reader.look.muted
                            opacity: 0.5
                        }
                    }
                    Text {
                        visible: modelData.kind === "break"
                        anchors.centerIn: parent
                        text: "⁂"
                        color: reader.look.muted
                        font.pixelSize: reader.fontSize
                    }
                }
            }

            Item { width: 1; height: 40 }

            Row {
                visible: reader.blocks.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                ScButton {
                    text: qsTr("Previous")
                    iconName: "chevron-left"
                    enabled: reader.hasPrev
                    onClicked: reader.load(reader.index - 1)
                }
                ScButton {
                    variant: "primary"
                    text: reader.hasNext ? qsTr("Next chapter") : qsTr("Finished")
                    iconName: reader.hasNext ? "chevron-right" : "check"
                    onClicked: reader.hasNext ? reader.load(reader.index + 1) : reader.exit()
                }
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 260
            spacing: 14
            visible: reader.task !== null && (reader.task.loading || !reader.task.ok)
            Spinner {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: reader.task !== null && reader.task.loading
                color: reader.look.muted
                size: 28
            }
            Text {
                width: Math.min(420, reader.width - 48)
                visible: reader.task !== null && !reader.task.loading && !reader.task.ok
                text: reader.task ? reader.task.error : ""
                color: reader.look.text
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
            }
            ScButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: reader.task !== null && !reader.task.loading && !reader.task.ok
                text: qsTr("Try again")
                iconName: "refresh-cw"
                onClicked: reader.load(reader.index)
            }
        }
    }

    // Desktop: the bar returns when the mouse moves near the top.
    PointerMotion {
        onMoved: (position) => { if (position.y < 90 + reader.insetTop) reader.barsShown = true }
    }

    // The status bar area keeps the reading theme's colour.
    Rectangle {
        z: 1
        width: parent.width
        height: reader.insetTop
        visible: height > 0
        color: reader.look.bg
    }

    // Reading progress along the top edge.
    Rectangle {
        z: 2
        y: reader.insetTop
        height: 2
        width: parent.width * Math.min(1, Math.max(0, flick.contentY / Math.max(1, flick.contentHeight - flick.height)))
        color: Theme.accent
        opacity: 0.8
    }

    Rectangle {
        id: topBar
        z: 1
        width: parent.width
        height: 64 + reader.insetTop
        y: reader.barsShown ? 0 : -height
        color: Theme.alpha(reader.look.chrome, 0.94)
        Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.alpha(reader.look.text, 0.08) }

        Row {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: reader.insetTop / 2
            spacing: 12
            IconButton {
                iconName: "arrow-left"
                iconColor: reader.look.text
                tip: qsTr("Close (Esc)")
                onClicked: reader.exit()
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: reader.series.title || ""
                    color: reader.look.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                    width: Math.min(implicitWidth, reader.width - 90 - novelActions.width)
                    elide: Text.ElideRight
                }
                Text {
                    text: (reader.index + 1) + qsTr(" of ") + reader.chapters.length + " · " + (reader.unit.title || "")
                        + (reader.task && reader.task.ok && reader.task.result.offline ? qsTr(" · Downloaded") : "")
                    color: reader.look.muted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    width: Math.min(implicitWidth, reader.width - 90 - novelActions.width)
                    elide: Text.ElideRight
                }
            }
        }
        Row {
            id: novelActions
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: reader.insetTop / 2
            spacing: 4
            IconButton { iconName: "chevron-left"; iconColor: reader.look.text; enabled: reader.hasPrev; tip: qsTr("Previous chapter") + reader.keyHint("reader.prev"); onClicked: reader.load(reader.index - 1) }
            IconButton { iconName: "chevron-right"; iconColor: reader.look.text; enabled: reader.hasNext; tip: qsTr("Next chapter") + reader.keyHint("reader.next"); onClicked: reader.load(reader.index + 1) }
            IconButton { iconName: "list"; iconColor: reader.look.text; tip: qsTr("Chapters"); onClicked: chapterPopup.open() }
            IconButton {
                visible: !String(reader.series.sourceUrl || "").startsWith("file:")
                iconName: "message-circle"
                iconColor: reader.look.text
                tip: qsTr("Comments")
                onClicked: comments.open()
            }
            IconButton { iconName: "type"; iconColor: reader.look.text; tip: qsTr("Text settings"); onClicked: typePopup.open() }
            IconButton {
                iconName: Window.window && Window.window.visibility === Window.FullScreen ? "minimize" : "maximize"
                iconColor: reader.look.text
                tip: qsTr("Full screen") + reader.keyHint("reader.fullscreen")
                onClicked: reader.toggleFullScreen()
            }
        }
    }

    CommentsPanel {
        id: comments
        media: "novel"
        title: reader.series.title || ""
        url: reader.series.sourceUrl || ""
        unit: String(reader.unit.number || "")
        label: qsTr("Chapter ") + (reader.unit.number || "")
    }

    Popup {
        id: chapterPopup
        x: parent.width - width - 16
        y: 70 + reader.insetTop
        width: Math.min(360, parent.width - 32)
        height: Math.min(parent.height - 120, 600)
        padding: 6
        onOpened: chapterList.positionViewAtIndex(reader.index, ListView.Center)
        background: Rectangle { radius: Theme.radiusLg; color: Theme.surface; border.width: 1; border.color: Theme.borderStrong }
        contentItem: ListView {
            id: chapterList
            clip: true
            model: reader.chapters
            ScrollBar.vertical: ScScrollBar {}
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: chapterList.width
                height: 38
                onClicked: {
                    chapterPopup.close()
                    reader.load(index)
                }
                background: Rectangle { radius: Theme.radiusSm; color: index === reader.index ? Theme.surface3 : parent.hovered ? Theme.surface2 : "transparent" }
                contentItem: Text {
                    text: modelData.title
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: index === reader.index ? Font.DemiBold : Font.Normal
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }

    Popup {
        id: typePopup
        x: parent.width - width - 16
        y: 70 + reader.insetTop
        width: Math.min(340, parent.width - 32)
        padding: 20
        background: Rectangle { radius: Theme.radiusLg; color: Theme.surface; border.width: 1; border.color: Theme.borderStrong }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast }
                NumberAnimation { property: "y"; from: 58 + reader.insetTop; to: 70 + reader.insetTop; duration: Motion.normal; easing.type: Easing.OutCubic }
            }
        }
        contentItem: Column {
            spacing: 14
            Text { text: "Theme"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            Row {
                spacing: 10
                Repeater {
                    model: ["app", "paper", "sepia", "dark", "black"]
                    Rectangle {
                        required property string modelData
                        width: 48
                        height: 48
                        radius: 24
                        color: reader.themes[modelData].bg
                        border.width: (App.settings.novelTheme || "app") === modelData ? 2 : 1
                        border.color: (App.settings.novelTheme || "app") === modelData ? Theme.accent : Theme.borderStrong
                        Text {
                            anchors.centerIn: parent
                            text: "Aa"
                            color: reader.themes[modelData].text
                            font.family: Theme.fontSerif
                            font.pixelSize: 15
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.setSetting("novelTheme", modelData) }
                    }
                }
            }
            Text { text: "Font"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            Segmented {
                options: [{ value: "Literata", label: qsTr("Serif") }, { value: "DM Sans", label: qsTr("Sans") }, { value: "Space Grotesk", label: qsTr("Grotesk") }]
                value: reader.fontFamily
                onPicked: (v) => App.setSetting("novelFont", v)
            }
            Text { text: "Size · " + reader.fontSize; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider { width: parent.width; from: 13; to: 34; stepSize: 1; value: reader.fontSize; onMoved: App.setSetting("novelSize", value) }
            Text { text: "Line spacing · " + reader.lineHeight.toFixed(2); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider { width: parent.width; from: 1.2; to: 2.4; stepSize: 0.05; value: reader.lineHeight; onMoved: App.setSetting("novelLineHeight", value) }
            Text { text: "Width · " + reader.columnWidth; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider { width: parent.width; from: 480; to: 1200; stepSize: 10; value: App.settings.novelWidth || 720; onMoved: App.setSetting("novelWidth", value) }
            Row {
                spacing: 10
                Toggle { checked: App.settings.novelJustify === true; onToggled: App.setSetting("novelJustify", checked) }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Justify text"; color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
            }
        }
    }
}
