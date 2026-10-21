import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import ScanCore

// Full-window comic reader. Vertical strip (webtoons) or single pages
// (manga), with eased scrolling, auto-hiding controls, progress saved as you
// read and the next chapter fetched ahead of time.
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
    readonly property var panels: task && task.ok ? task.result.panels : []
    readonly property string referer: task && task.ok ? task.result.referer : ""

    // Settings chosen for this series alone (the reader's settings, "Only
    // this series") win over the general ones.
    readonly property var seriesPrefs: {
        const all = App.settings.readerSeries || {}
        return series.id && all[series.id] ? all[series.id] : null
    }
    function opt(name, fallback) {
        if (seriesPrefs && seriesPrefs[name] !== undefined) return seriesPrefs[name]
        const value = App.settings[name]
        return value === undefined || value === null ? fallback : value
    }
    function setOpt(name, value) {
        if (!seriesPrefs) {
            App.setSetting(name, value)
            return
        }
        const all = Object.assign({}, App.settings.readerSeries || {})
        const mine = Object.assign({}, seriesPrefs)
        mine[name] = value
        all[series.id] = mine
        App.setSetting("readerSeries", all)
    }
    function setSeriesOnly(on) {
        const all = Object.assign({}, App.settings.readerSeries || {})
        if (on)
            all[series.id] = { readerMode: mode, readerDirection: direction, readerWidth: opt("readerWidth", 820),
                               readerGap: gap, readerSpread: opt("readerSpread", "single"), readerCrop: crop }
        else
            delete all[series.id]
        App.setSetting("readerSeries", all)
    }

    readonly property string mode: opt("readerMode", "strip") === "paged" ? "paged" : "strip"
    readonly property string direction: opt("readerDirection", "ltr") === "rtl" ? "rtl" : "ltr"
    readonly property int gap: opt("readerGap", 0)
    readonly property bool crop: opt("readerCrop", false) === true
    readonly property int stripWidth: Math.min(width, opt("readerWidth", 820))
    property int currentPanel: 0
    property bool controlsShown: true
    property bool finished: false
    property var resumePosition: null

    // Drawn under the phone's status and navigation bars; the controls are
    // padded by these insets instead (see the immersive stack in Main.qml).
    readonly property bool edgeToEdge: true
    readonly property real insetTop: SafeArea.margins.top
    readonly property real insetBottom: SafeArea.margins.bottom

    color: "#000000"
    focus: true

    function load(i, fromEnd) {
        saveProgress(true)
        index = i
        finished = false
        currentPanel = 0
        const last = Library.progressMap(series.id)[chapters[i].key]
        resumePosition = last && !last.completed && last.position ? last.position : null
        if (nextTask && nextTask.ok && nextTask.done && nextFor === i) task = nextTask
        else task = Comics.panels(chapters[i].url, series.id, chapters[i].key)
        nextTask = null
        strip.contentY = strip.originY
        pageView.resetZoom()
        pageView.page = 0
        showControls()
        Presence.show("comic", series.title || "", chapters[i].title || "")
    }
    property int nextFor: -1

    Component.onCompleted: {
        load(index)
        Activity.setMedia("comic")
    }
    Component.onDestruction: {
        Activity.setMedia("")
        Presence.clear()
        saveProgress(true)
    }

    Connections {
        target: reader.task
        function onFinished() {
            Qt.callLater(() => {
                if (reader.resumePosition && reader.resumePosition.panel > 0) {
                    const p = Math.min(reader.resumePosition.panel, reader.panels.length - 1)
                    if (reader.mode === "strip") strip.positionViewAtIndex(p, ListView.Beginning)
                    else pageView.page = pageView.views[pageView.viewOf(p)][0]
                    reader.currentPanel = p
                }
            })
        }
        function onFailed(error) { reader.showControls() }
    }

    // ------------------------------------------------------------ progress

    Timer {
        id: saveTimer
        interval: 600
        onTriggered: reader.commitProgress()
    }
    function commitProgress() {
        if (!series.id || !unit.key || panels.length === 0) return
        const percent = finished ? 1 : (currentPanel + 1) / panels.length
        Library.recordProgress(series, unit, { type: reader.mode, panel: currentPanel }, percent, finished || percent >= 0.98)
    }
    function saveProgress(force) {
        if (!series.id || !unit.key || panels.length === 0) return
        if (force) {
            saveTimer.stop()
            commitProgress()
        } else {
            saveTimer.restart()
        }
    }

    onCurrentPanelChanged: {
        saveProgress(false)
        if (panels.length > 0 && currentPanel >= panels.length * 0.7) prefetchNext()
    }

    function prefetchNext() {
        if (!hasNext || nextFor === index + 1) return
        nextFor = index + 1
        nextTask = Comics.panels(chapters[index + 1].url, series.id, chapters[index + 1].key)
    }
    Connections {
        target: reader.nextTask
        function onFinished(result) { Comics.prefetch(result.panels, result.referer, 6) }
    }

    function markFinished() {
        if (finished) return
        finished = true
        saveProgress(true)
    }

    // ------------------------------------------------------------ controls

    function showControls() {
        controlsShown = true
        hideTimer.restart()
    }
    function toggleControls() {
        if (controlsShown) controlsShown = false
        else showControls()
    }
    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: {
            // Stay up while the pointer rests on the controls, a popup is
            // open, or there are no pages yet (loading, or an error to act on).
            if (settingsPopup.opened || chapterPopup.opened || comments.opened || topHover.hovered
                || reader.panels.length === 0)
                restart()
            else
                reader.controlsShown = false
        }
    }

    function scrollBy(delta) {
        if (mode === "strip") {
            const base = scrollAnim.running ? scrollAnim.to : strip.contentY
            const target = Math.max(strip.originY, Math.min(strip.contentHeight + strip.originY - strip.height, base + delta))
            scrollAnim.to = target
            scrollAnim.restart()
        } else {
            pageView.turn(delta > 0 ? 1 : -1)
        }
    }

    function exit() {
        saveProgress(true)
        Router.back()
    }

    function keyHint(action) {
        const keys = Keymap.labels(action)
        return keys.length ? " (" + keys[0] + ")" : ""
    }

    // Scrolling keys come from Settings → Keyboard too, but only count
    // while the reader has focus (not in its chapter list, say).
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) exit()
        else if (Keymap.matches("reader.down", event)) scrollBy(height * 0.18)
        else if (Keymap.matches("reader.up", event)) scrollBy(-height * 0.18)
        else if (Keymap.matches("reader.pageDown", event)) scrollBy(height * 0.85)
        else if (Keymap.matches("reader.pageUp", event)) scrollBy(-height * 0.85)
        else if (event.key === Qt.Key_Right) mode === "paged" ? pageView.turn(direction === "rtl" ? -1 : 1) : (hasNext && load(index + 1))
        else if (event.key === Qt.Key_Left) mode === "paged" ? pageView.turn(direction === "rtl" ? 1 : -1) : (hasPrev && load(index - 1))
        else return
        event.accepted = true
    }

    // The rest come from Settings → Keyboard (see Keymap).
    Shortcut { sequences: Keymap.sequences("reader.next"); onActivated: if (reader.hasNext) reader.load(reader.index + 1) }
    Shortcut { sequences: Keymap.sequences("reader.prev"); onActivated: if (reader.hasPrev) reader.load(reader.index - 1) }
    Shortcut {
        sequences: Keymap.sequences("reader.first")
        onActivated: reader.mode === "strip" ? strip.positionViewAtBeginning() : (pageView.page = 0)
    }
    Shortcut {
        sequences: Keymap.sequences("reader.last")
        onActivated: reader.mode === "strip" ? strip.positionViewAtEnd() : (pageView.page = pageView.views[pageView.views.length - 1][0])
    }
    Shortcut {
        sequences: Keymap.sequences("reader.fullscreen")
        onActivated: Window.window.visibility === Window.FullScreen ? Window.window.showNormal() : Window.window.showFullScreen()
    }
    Shortcut { sequences: Keymap.sequences("reader.mode"); onActivated: reader.setOpt("readerMode", reader.mode === "strip" ? "paged" : "strip") }
    Shortcut { sequences: Keymap.sequences("reader.wider"); onActivated: reader.setOpt("readerWidth", Math.min(2400, reader.stripWidth + 80)) }
    Shortcut { sequences: Keymap.sequences("reader.narrower"); onActivated: reader.setOpt("readerWidth", Math.max(360, reader.stripWidth - 80)) }

    // ------------------------------------------------------------ strip

    ListView {
        id: strip
        anchors.fill: parent
        visible: reader.mode === "strip"
        model: reader.mode === "strip" ? reader.panels : []
        cacheBuffer: Math.max(2400, height * 3)
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        maximumFlickVelocity: 7000
        flickDeceleration: 2600
        spacing: reader.gap
        reuseItems: false
        ScrollBar.vertical: ScScrollBar { policy: reader.controlsShown ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded }

        onContentYChanged: {
            const i = indexAt(width / 2, contentY + height * 0.35)
            if (i >= 0) reader.currentPanel = i
            if (atYEnd && reader.panels.length > 0) reader.markFinished()
            if (moving || scrollAnim.running) reader.controlsShown = false
        }

        NumberAnimation {
            id: scrollAnim
            target: strip
            property: "contentY"
            duration: 300
            easing.type: Easing.OutCubic
        }

        // Phone: a tap toggles the controls; on desktop single clicks are left
        // alone and a double click does. On the list itself, which takes
        // touches before anything behind it.
        TapHandler {
            onSingleTapped: if (App.platform === "android") reader.toggleControls()
            onDoubleTapped: if (App.platform !== "android") reader.toggleControls()
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse
            onWheel: (event) => {
                if (event.modifiers & Qt.ControlModifier) {
                    reader.setOpt("readerWidth", Math.max(360, Math.min(2400, reader.stripWidth + (event.angleDelta.y > 0 ? 60 : -60))))
                    return
                }
                if (event.phase !== Qt.NoScrollPhase || event.pixelDelta.y !== 0) {
                    scrollAnim.stop()
                    const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y
                    strip.contentY = Math.max(strip.originY, Math.min(strip.contentHeight + strip.originY - strip.height, strip.contentY - dy))
                    return
                }
                reader.scrollBy(-event.angleDelta.y / 120 * 180)
            }
        }

        delegate: Item {
            id: panelItem
            required property var modelData
            required property int index
            width: strip.width
            height: image.status === PanelImage.Ready && image.sourceWidth > 0
                ? reader.stripWidth * image.sourceHeight / image.sourceWidth
                : (modelData.width > 0 && modelData.height > 0 ? reader.stripWidth * modelData.height / modelData.width : reader.stripWidth * 1.4)

            PanelImage {
                id: image
                anchors.horizontalCenter: parent.horizontalCenter
                width: reader.stripWidth
                height: parent.height
                source: panelItem.modelData.url
                referer: reader.referer
                maxDecodeWidth: Math.ceil(Math.min(2400, reader.stripWidth * Screen.devicePixelRatio))
                opacity: status === PanelImage.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Motion.normal } }
            }
            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: image.status !== PanelImage.Ready
                Spinner {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: image.status === PanelImage.Loading
                    color: "#7a7a80"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: image.status === PanelImage.Error
                    text: qsTr("Page ") + (panelItem.index + 1) + qsTr(" did not load")
                    color: "#a0a0a8"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: image.status === PanelImage.Error
                    text: qsTr("Retry")
                    compact: true
                    onClicked: image.reload()
                }
            }
        }

        footer: Item {
            width: strip.width
            height: Math.max(320, strip.height * 0.6)
            visible: reader.panels.length > 0
            NextChapterCard {
                anchors.centerIn: parent
                host: reader
            }
        }
    }

    // ------------------------------------------------------------ paged

    Item {
        id: pageView
        anchors.fill: parent
        visible: reader.mode === "paged"
        // The first page of the pages shown.
        property int page: 0
        property int direction: 1
        readonly property bool rtl: reader.direction === "rtl"

        // Two pages side by side in a wide window, when chosen: the cover
        // alone, then pairs, and a landscape page (already a spread) alone.
        readonly property bool twoUp: reader.opt("readerSpread", "single") === "double" && width > height * 1.15
        // Pages found to be landscape once loaded (many sources give no
        // sizes up front); the pages ahead load early, so this is usually
        // known before they are shown.
        property var widePages: ({})
        function noteShape(i, w, h) {
            if (i < 0 || w <= 0 || h <= 0) return
            const wide = w > h * 1.1
            if (!!widePages[i] === wide) return
            const next = Object.assign({}, widePages)
            next[i] = wide
            widePages = next
        }
        Connections {
            target: reader
            function onPanelsChanged() { pageView.widePages = {} }
        }
        readonly property var views: {
            const out = []
            const n = reader.panels.length
            const wide = (i) => {
                if (widePages[i] !== undefined) return widePages[i]
                const p = reader.panels[i]
                return p && p.width > 0 && p.height > 0 && p.width > p.height * 1.1
            }
            let i = 0
            while (i < n) {
                if (twoUp && i > 0 && i + 1 < n && !wide(i) && !wide(i + 1)) {
                    out.push([i, i + 1])
                    i += 2
                } else {
                    out.push([i])
                    i += 1
                }
            }
            return out.length ? out : [[0]]
        }
        function viewOf(p) {
            for (let v = 0; v < views.length; ++v)
                if (views[v].indexOf(p) >= 0) return v
            return 0
        }
        readonly property int view: viewOf(page)
        readonly property var shown: views[view] || [page]
        // Turning to one or two pages keeps the same first page in view.
        onTwoUpChanged: page = views[viewOf(page)][0]

        function turn(step) {
            const next = view + step
            if (next < 0) {
                if (reader.hasPrev) reader.load(reader.index - 1)
                return
            }
            if (next >= views.length) {
                reader.markFinished()
                if (reader.hasNext) reader.load(reader.index + 1)
                return
            }
            direction = step
            page = views[next][0]
        }
        onPageChanged: {
            resetZoom()
            reader.currentPanel = shown[shown.length - 1]
            if (view === views.length - 1) reader.markFinished()
            pageSwap.restart()
        }

        // ---- zoom: pinch, double-click the middle, or Ctrl + wheel
        readonly property bool zoomed: zoomLayer.scale > 1.01
        function clampPan() {
            zoomLayer.x = Math.min(0, Math.max(pageHolder.width * (1 - zoomLayer.scale), zoomLayer.x))
            zoomLayer.y = Math.min(0, Math.max(pageHolder.height * (1 - zoomLayer.scale), zoomLayer.y))
        }
        function zoomAt(px, py, target) {
            const z = Math.max(1, Math.min(4, target))
            const k = z / zoomLayer.scale
            zoomLayer.x = px - (px - zoomLayer.x) * k
            zoomLayer.y = py - (py - zoomLayer.y) * k
            zoomLayer.scale = z
            clampPan()
        }
        function resetZoom() {
            zoomAnim.stop()
            zoomLayer.scale = 1
            zoomLayer.x = 0
            zoomLayer.y = 0
        }
        function toggleZoom(px, py) {
            const z = zoomed ? 1 : 2.2
            const k = z / zoomLayer.scale
            zoomAnim.toScale = z
            zoomAnim.toX = z === 1 ? 0 : Math.min(0, Math.max(pageHolder.width * (1 - z), px - (px - zoomLayer.x) * k))
            zoomAnim.toY = z === 1 ? 0 : Math.min(0, Math.max(pageHolder.height * (1 - z), py - (py - zoomLayer.y) * k))
            zoomAnim.restart()
        }
        ParallelAnimation {
            id: zoomAnim
            property real toScale: 1
            property real toX: 0
            property real toY: 0
            NumberAnimation { target: zoomLayer; property: "scale"; to: zoomAnim.toScale; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { target: zoomLayer; property: "x"; to: zoomAnim.toX; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { target: zoomLayer; property: "y"; to: zoomAnim.toY; duration: Motion.normal; easing.type: Easing.OutCubic }
        }

        Item {
            id: pageHolder
            width: parent.width
            height: parent.height
            Item {
                id: zoomLayer
                width: pageHolder.width
                height: pageHolder.height
                transformOrigin: Item.TopLeft
                // One page centred, or two meeting in the middle (in reading
                // order: right to left puts the first on the right).
                Item {
                    id: pages
                    anchors.fill: parent
                    readonly property bool two: pageView.shown.length > 1
                    readonly property real slot: two ? width / 2 : width
                    readonly property real total: pageImage.width + (two ? secondImage.width : 0)
                    PanelImage {
                        id: pageImage
                        readonly property real ratio: sourceWidth > 0 ? sourceHeight / sourceWidth : 1.45
                        width: Math.min(pages.slot, pages.height / ratio)
                        height: width * ratio
                        x: !pages.two ? (pages.width - width) / 2
                            : pageView.rtl ? pages.width / 2 : pages.width / 2 - width
                        y: (pages.height - height) / 2
                        source: reader.mode === "paged" && reader.panels.length > 0 ? reader.panels[Math.min(pageView.shown[0], reader.panels.length - 1)].url : ""
                        referer: reader.referer
                        cropBorders: reader.crop
                        maxDecodeWidth: 2400
                        onLoaded: pageView.noteShape(pageView.shown[0], sourceWidth, sourceHeight)
                    }
                    PanelImage {
                        id: secondImage
                        readonly property real ratio: sourceWidth > 0 ? sourceHeight / sourceWidth : 1.45
                        visible: pages.two
                        width: Math.min(pages.slot, pages.height / ratio)
                        height: width * ratio
                        x: pageView.rtl ? pages.width / 2 - width : pages.width / 2
                        y: (pages.height - height) / 2
                        source: pages.two && reader.mode === "paged" ? reader.panels[pageView.shown[1]].url : ""
                        referer: reader.referer
                        cropBorders: reader.crop
                        maxDecodeWidth: 2400
                        onLoaded: pageView.noteShape(pageView.shown[1], sourceWidth, sourceHeight)
                    }
                }
            }
            Spinner {
                anchors.centerIn: parent
                visible: pageImage.status === PanelImage.Loading || secondImage.status === PanelImage.Loading
                color: "#7a7a80"
            }
            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: pageImage.status === PanelImage.Error || secondImage.status === PanelImage.Error
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Page ") + (pageView.shown[pageImage.status === PanelImage.Error ? 0 : pageView.shown.length - 1] + 1) + qsTr(" did not load")
                    color: "#a0a0a8"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Retry")
                    compact: true
                    onClicked: {
                        if (pageImage.status === PanelImage.Error) pageImage.reload()
                        if (secondImage.status === PanelImage.Error) secondImage.reload()
                    }
                }
            }
        }
        // Preload the next pages and the one before.
        Repeater {
            model: {
                if (reader.mode !== "paged") return []
                const last = pageView.shown[pageView.shown.length - 1]
                return [last + 1, last + 2, last + 3, pageView.shown[0] - 1]
            }
            PanelImage {
                required property var modelData
                visible: false
                width: 1
                height: 1
                source: modelData >= 0 && modelData < reader.panels.length ? reader.panels[modelData].url : ""
                referer: reader.referer
                cropBorders: reader.crop
                maxDecodeWidth: 2400
                onLoaded: pageView.noteShape(modelData, sourceWidth, sourceHeight)
            }
        }
        ParallelAnimation {
            id: pageSwap
            NumberAnimation { target: pageHolder; property: "opacity"; from: 0.35; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { target: pageHolder; property: "x"; from: 26 * pageView.direction * (pageView.rtl ? -1 : 1); to: 0; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            // Sides turn the page (unless zoomed in); the middle shows the
            // controls, and a double click there zooms.
            onClicked: (mouse) => {
                const third = width / 3
                if (!pageView.zoomed && mouse.x < third) pageView.turn(pageView.rtl ? 1 : -1)
                else if (!pageView.zoomed && mouse.x > third * 2) pageView.turn(pageView.rtl ? -1 : 1)
                else reader.controlsShown ? (reader.controlsShown = false) : reader.showControls()
            }
            onDoubleClicked: (mouse) => {
                if (pageView.zoomed || (mouse.x >= width / 3 && mouse.x <= width * 2 / 3)) pageView.toggleZoom(mouse.x, mouse.y)
            }
            onWheel: (wheel) => {
                if (wheel.modifiers & Qt.ControlModifier) {
                    pageView.zoomAt(wheel.x, wheel.y, zoomLayer.scale * (wheel.angleDelta.y > 0 ? 1.2 : 1 / 1.2))
                } else if (pageView.zoomed) {
                    zoomLayer.x += wheel.angleDelta.x / 120 * 90
                    zoomLayer.y += wheel.angleDelta.y / 120 * 90
                    pageView.clampPan()
                } else {
                    pageView.turn(wheel.angleDelta.y < 0 ? 1 : -1)
                }
            }

            PinchHandler {
                id: pinch
                target: null
                property real startScale: 1
                onActiveChanged: if (active) startScale = zoomLayer.scale
                onActiveScaleChanged: if (active) pageView.zoomAt(centroid.position.x, centroid.position.y, startScale * activeScale)
            }

            // Zoomed in: dragging moves around the page.
            DragHandler {
                id: pan
                target: null
                enabled: pageView.zoomed
                property point start
                onActiveChanged: if (active) start = Qt.point(zoomLayer.x, zoomLayer.y)
                onTranslationChanged: if (active) {
                    zoomLayer.x = start.x + translation.x
                    zoomLayer.y = start.y + translation.y
                    pageView.clampPan()
                }
            }

            // Swipe (or drag) sideways to turn the page; the page follows
            // and springs back from a short swipe. Inside the MouseArea, so it
            // sees the press before the MouseArea takes it.
            DragHandler {
                id: swipe
                target: null
                enabled: !pageView.zoomed && !pinch.active
                yAxis.enabled: false
                onTranslationChanged: if (active) pageHolder.x = translation.x * 0.7
                onActiveChanged: {
                    if (active) {
                        pageSwap.stop()
                        return
                    }
                    const dx = translation.x
                    if (Math.abs(dx) > Math.min(90, pageView.width * 0.12)) {
                        const forward = dx < 0 ? !pageView.rtl : pageView.rtl
                        pageView.turn(forward ? 1 : -1)
                    }
                    if (!pageSwap.running) springBack.start()
                }
            }
            NumberAnimation { id: springBack; target: pageHolder; property: "x"; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
        }
    }

    // ------------------------------------------------------------ states

    Column {
        anchors.centerIn: parent
        spacing: 14
        visible: reader.task !== null && (reader.task.loading || !reader.task.ok)
        Spinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: reader.task !== null && reader.task.loading
            size: 30
            color: "#9a9aa2"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: reader.task !== null && reader.task.loading
            text: reader.unit.title || ""
            color: "#9a9aa2"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
        }
        Text {
            width: Math.min(420, reader.width - 48)
            visible: reader.task !== null && !reader.task.loading && !reader.task.ok
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: reader.task ? reader.task.error : ""
            color: "#d4d4d8"
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

    // Desktop: moving the mouse reveals the controls.
    PointerMotion {
        onMoved: reader.showControls()
    }

    // The page progress bar: one thin, flat segment per page flush with the
    // left edge, as on scancore.onl: read pages dark, unread grey, the page
    // you are on white. Tap one to go
    // to that page.
    function jumpTo(i) {
        if (i < 0 || i >= reader.panels.length) return
        if (reader.mode === "strip") strip.positionViewAtIndex(i, ListView.Beginning)
        else pageView.page = pageView.views[pageView.viewOf(i)][0]
    }
    Item {
        id: rail
        // Above the controls' shading, so it reads the same with them shown.
        z: 50
        visible: App.settings.readerProgressBar !== false && reader.panels.length > 0
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: reader.insetTop
        // Wider than what it paints, so the segments are easy to hit.
        width: 12
        readonly property int count: reader.panels.length
        readonly property real gap: count > 0 && (height - (count - 1)) / count >= 3 ? 1 : 0
        readonly property real segment: count > 0 ? (height - (count - 1) * gap) / count : 0
        Repeater {
            model: rail.count
            Rectangle {
                required property int index
                // Whole pixels, so the 1 px gaps stay visible.
                readonly property int startY: Math.round(index * (rail.segment + rail.gap))
                readonly property int endY: Math.round((index + 1) * (rail.segment + rail.gap))
                y: startY
                width: 3
                height: Math.max(1, endY - startY - rail.gap)
                color: index === reader.currentPanel ? "#ffffff"
                     : index < reader.currentPanel || reader.finished ? "#18181b" : "#71717a"
                Behavior on color { ColorAnimation { duration: Motion.fast } }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: (mouse) => reader.jumpTo(Math.min(rail.count - 1, Math.floor(mouse.y / (rail.segment + rail.gap))))
        }
    }

    // ------------------------------------------------------------ top bar

    // Keeps the status bar legible over light pages while the controls are hidden.
    Rectangle {
        width: parent.width
        height: reader.insetTop
        visible: height > 0
        color: Qt.rgba(0, 0, 0, 0.6)
    }

    Item {
        id: topBar
        // Phones: right under the status bar, as in the novel reader.
        readonly property int pad: Theme.compact ? 8 : 16
        width: parent.width
        height: (Theme.compact ? 64 : 76) + reader.insetTop
        y: reader.controlsShown ? 0 : -height
        opacity: reader.controlsShown ? 1 : 0
        Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
        HoverHandler { id: topHover; enabled: App.platform !== "android" }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.85) }
                // Dark through the buttons, so the bar reads as one piece
                // with the status bar, then fading into the page.
                GradientStop { position: (reader.insetTop + topBar.pad + 36) / topBar.height; color: Qt.rgba(0, 0, 0, 0.7) }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0) }
            }
        }
        Row {
            x: 18
            y: topBar.pad + reader.insetTop
            spacing: 12
            IconButton {
                iconName: "arrow-left"
                iconColor: "white"
                tip: qsTr("Close reader (Esc)")
                onClicked: reader.exit()
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Text {
                    text: reader.series.title || ""
                    color: "white"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                    width: Math.min(implicitWidth, reader.width - 100 - topActions.width)
                    elide: Text.ElideRight
                }
                Text {
                    text: (reader.unit.title || "")
                          + (reader.panels.length ? "  ·  " + (reader.currentPanel + 1) + " / " + reader.panels.length : "")
                          + (reader.task && reader.task.ok && reader.task.result.offline ? qsTr("  ·  Downloaded") : "")
                    color: "#b4b4bb"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    width: Math.min(implicitWidth, reader.width - 100 - topActions.width)
                    elide: Text.ElideRight
                }
            }
        }
        Row {
            id: topActions
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: topBar.pad + reader.insetTop
            spacing: 4
            // Pages are on the progress bar at the left edge; chapters here.
            IconButton {
                iconName: "skip-back"
                iconColor: "white"
                enabled: reader.hasPrev
                opacity: enabled ? 1 : 0.4
                tip: qsTr("Previous chapter") + reader.keyHint("reader.prev")
                onClicked: reader.load(reader.index - 1)
            }
            IconButton {
                iconName: "skip-forward"
                iconColor: "white"
                enabled: reader.hasNext
                opacity: enabled ? 1 : 0.4
                tip: qsTr("Next chapter") + reader.keyHint("reader.next")
                onClicked: reader.load(reader.index + 1)
            }
            IconButton {
                iconName: "list"
                iconColor: "white"
                tip: qsTr("Chapters")
                onClicked: chapterPopup.open()
            }
            IconButton {
                iconName: reader.mode === "strip" ? "move-vertical" : "layers"
                iconColor: "white"
                tip: (reader.mode === "strip" ? qsTr("Switch to pages") : qsTr("Switch to vertical strip")) + reader.keyHint("reader.mode")
                onClicked: reader.setOpt("readerMode", reader.mode === "strip" ? "paged" : "strip")
            }
            IconButton {
                visible: !String(reader.series.sourceUrl || "").startsWith("file:")
                iconName: "message-circle"
                iconColor: "white"
                tip: qsTr("Comments")
                onClicked: comments.open()
            }
            IconButton {
                iconName: "sliders-horizontal"
                iconColor: "white"
                tip: qsTr("Reader settings")
                onClicked: settingsPopup.open()
            }
            IconButton {
                iconName: Window.window && Window.window.visibility === Window.FullScreen ? "minimize" : "maximize"
                iconColor: "white"
                tip: qsTr("Full screen") + reader.keyHint("reader.fullscreen")
                onClicked: Window.window.visibility === Window.FullScreen ? Window.window.showNormal() : Window.window.showFullScreen()
            }
        }
    }

    // ------------------------------------------------------------ popups

    Popup {
        id: chapterPopup
        x: parent.width - width - 18
        y: 70 + reader.insetTop
        width: Math.min(340, parent.width - 36)
        height: Math.min(parent.height - 160 - reader.insetTop - reader.insetBottom, 560)
        padding: 6
        onOpened: chapterList.positionViewAtIndex(reader.index, ListView.Center)
        background: Rectangle { radius: Theme.radiusLg; color: "#151517"; border.width: 1; border.color: "#2a2a30" }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast } }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }
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
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: index === reader.index ? "#2a2a30" : parent.hovered ? "#1f1f23" : "transparent"
                }
                contentItem: Text {
                    text: modelData.title
                    color: index === reader.index ? "white" : "#c4c4cb"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: index === reader.index ? Font.DemiBold : Font.Normal
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }

    CommentsPanel {
        id: comments
        media: "comic"
        title: (reader.series.extra && reader.series.extra.catalogTitle) || reader.series.title || ""
        comickHid: (reader.series.extra && reader.series.extra.hid) || reader.series.hid || ""
        url: reader.series.sourceUrl || ""
        unit: String(reader.unit.number || "")
        label: qsTr("Chapter ") + (reader.unit.number || "")
    }

    Popup {
        id: settingsPopup
        x: parent.width - width - 18
        y: 70 + reader.insetTop
        width: Math.min(320, parent.width - 36)
        padding: 18
        background: Rectangle { radius: Theme.radiusLg; color: "#151517"; border.width: 1; border.color: "#2a2a30" }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast }
                NumberAnimation { property: "y"; from: 60 + reader.insetTop; to: 70 + reader.insetTop; duration: Motion.normal; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }
        contentItem: Column {
            spacing: 16
            Text { text: "Layout"; color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            Segmented {
                options: [{ value: "strip", label: qsTr("Vertical"), icon: "move-vertical" }, { value: "paged", label: qsTr("Pages"), icon: "layers" }]
                value: reader.mode
                onPicked: (v) => reader.setOpt("readerMode", v)
            }
            Segmented {
                visible: reader.mode === "paged"
                options: [{ value: "ltr", label: qsTr("Left to right") }, { value: "rtl", label: qsTr("Right to left") }]
                value: reader.direction
                onPicked: (v) => reader.setOpt("readerDirection", v)
            }
            Segmented {
                visible: reader.mode === "paged" && App.platform !== "android"
                options: [{ value: "single", label: qsTr("One page") }, { value: "double", label: qsTr("Two pages") }]
                value: reader.opt("readerSpread", "single")
                onPicked: (v) => reader.setOpt("readerSpread", v)
            }
            Row {
                visible: reader.mode === "paged"
                spacing: 10
                Toggle {
                    checked: reader.crop
                    onToggled: reader.setOpt("readerCrop", checked)
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Trim white or black borders"; color: "#d4d4d8"; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
            }
            Text { visible: reader.mode === "strip"; text: "Width · " + reader.stripWidth + " px"; color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider {
                visible: reader.mode === "strip"
                width: parent.width
                from: 360
                to: 2000
                stepSize: 20
                value: reader.stripWidth
                onMoved: reader.setOpt("readerWidth", value)
            }
            Text { visible: reader.mode === "strip"; text: "Gap between panels · " + reader.gap + " px"; color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider {
                visible: reader.mode === "strip"
                width: parent.width
                from: 0
                to: 40
                stepSize: 2
                value: reader.gap
                onMoved: reader.setOpt("readerGap", value)
            }
            Row {
                spacing: 10
                Toggle {
                    checked: App.settings.readerProgressBar !== false
                    onToggled: App.setSetting("readerProgressBar", checked)
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Page progress bar"); color: "#d4d4d8"; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
            }
            Rectangle { width: parent.width; height: 1; color: "#2a2a30" }
            Row {
                visible: !!reader.series.id
                spacing: 10
                Toggle {
                    checked: reader.seriesPrefs !== null
                    onToggled: reader.setSeriesOnly(checked)
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text { text: "Only for this series"; color: "#d4d4d8"; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    Text { text: "Keeps these settings for " + (reader.series.title || "it") + " alone"; width: 230; elide: Text.ElideRight; color: "#8a8a92"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs + 1 }
                }
            }
        }
    }
}
