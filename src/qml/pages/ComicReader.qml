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
    // Colour filter: brightness and warmth are shades over the page (they
    // change live); grey and invert are applied while pages decode.
    readonly property real brightness: Math.max(0.2, Math.min(1, Number(opt("readerBrightness", 1))))
    readonly property real warmth: Math.max(0, Math.min(1, Number(opt("readerWarmth", 0))))
    readonly property string imageFilter: [opt("readerGrey", false) === true ? "grey" : "",
                                           opt("readerInvert", false) === true ? "invert" : ""].filter(f => f).join(" ")
    // Pages: a wide spread shown as its two halves, one after the other.
    readonly property bool splitWide: opt("readerSplitWide", false) === true
    readonly property string tapZones: opt("readerTapZones", "sides")
    readonly property string tapInvert: opt("readerTapInvert", "none")
    readonly property bool stripTaps: opt("readerStripTaps", false) === true
    // Holding a finger (or the mouse) on the page magnifies it twice, under
    // the finger or at the middle, until it lets go: small dialogue and
    // margin notes. Double tap (double click) zooms in on paged pages.
    readonly property bool longPressZoom: opt("readerLongPressZoom", true) !== false
    readonly property bool longPressAtCentre: opt("readerLongPressZoomAt", "press") === "centre"
    readonly property bool doubleTapZoom: opt("readerDoubleTapZoom", true) !== false
    property bool magnifying: false
    property point magnifyAt: Qt.point(0, 0)
    function startMagnifier(item, x, y) {
        if (!longPressZoom) return
        magnifyAt = longPressAtCentre ? Qt.point(item.width / 2, item.height / 2) : Qt.point(x, y)
        magnifying = true
        controlsShown = false
    }
    function moveMagnifier(x, y) { if (magnifying && !longPressAtCentre) magnifyAt = Qt.point(x, y) }
    readonly property bool volumeKeys: App.platform === "android" && opt("readerVolumeKeys", false) === true
    readonly property var tapZoneOptions: [{ value: "sides", label: qsTr("Tap zones: left and right") }, { value: "lshape", label: qsTr("Tap zones: L shape") },
        { value: "kindle", label: qsTr("Tap zones: Kindle") }, { value: "edge", label: qsTr("Tap zones: edges") }, { value: "off", label: qsTr("Tap zones: off") }]
    readonly property var tapInvertOptions: [{ value: "none", label: qsTr("Not flipped") }, { value: "horizontal", label: qsTr("Flipped left and right") },
        { value: "vertical", label: qsTr("Flipped top and bottom") }, { value: "both", label: qsTr("Flipped both ways") }]
    onVolumeKeysChanged: App.setVolumeKeys(volumeKeys)
    property int currentPanel: 0
    property bool controlsShown: true
    property bool finished: false
    property var resumePosition: null

    // Drawn under the phone's status and navigation bars; the controls are
    // padded by these insets instead (see the immersive stack in Main.qml).
    readonly property bool edgeToEdge: true
    // Hidden bars wait above the top edge: never let them show outside.
    clip: true
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

    // The screen stays on while reading (a setting, on by default).
    readonly property bool keepScreenOn: App.settings.readerKeepAwake !== false && Qt.application.state === Qt.ApplicationActive
    onKeepScreenOnChanged: App.keepAwake("reader", keepScreenOn)

    Component.onCompleted: {
        load(index)
        App.keepAwake("reader", keepScreenOn)
        Activity.setMedia("comic")
        App.setVolumeKeys(volumeKeys)
    }
    Component.onDestruction: {
        App.keepAwake("reader", false)
        App.setVolumeKeys(false)
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
        // E-ink: a black frame between pages clears the last one's ghost.
        if (Theme.eink && mode === "paged" && App.settings.einkFlash !== false) einkFlash.start()
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

    // Where a tap lands: "prev", "next" or "menu". Zones are laid out for
    // left-to-right reading and mirrored for right to left; the invert
    // setting flips them on top of that.
    readonly property var tapLayouts: ({
        sides:  ["prev", "menu", "next", "prev", "menu", "next", "prev", "menu", "next"],
        lshape: ["prev", "prev", "prev", "prev", "menu", "next", "next", "next", "next"],
        kindle: ["menu", "menu", "menu", "prev", "next", "next", "prev", "next", "next"],
        edge:   ["next", "menu", "next", "next", "menu", "next", "next", "prev", "next"],
        off:    ["menu", "menu", "menu", "menu", "menu", "menu", "menu", "menu", "menu"]
    })
    function tapZone(x, y, w, h) {
        const flipX = tapInvert === "horizontal" || tapInvert === "both"
        if (flipX !== (mode === "paged" && direction === "rtl")) x = w - x
        if (tapInvert === "vertical" || tapInvert === "both") y = h - y
        const c = Math.max(0, Math.min(2, Math.floor(x * 3 / w)))
        const r = Math.max(0, Math.min(2, Math.floor(y * 3 / h)))
        return (tapLayouts[tapZones] || tapLayouts.sides)[r * 3 + c]
    }
    function act(action) {
        if (action === "menu") {
            toggleControls()
            return
        }
        const step = action === "next" ? 1 : -1
        if (mode === "paged") pageView.turn(step)
        else scrollBy(step * height * 0.85)
    }
    Connections {
        target: App
        function onVolumeKey(down) { reader.act(down ? "next" : "prev") }
    }

    function scrollBy(delta) {
        if (mode === "strip") {
            stripWheel.scrollTo(stripWheel.ease.running ? stripWheel.target + delta : strip.contentY + delta)
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
        onActivated: fullScreen.toggle()
    }
    ReaderWindow { id: fullScreen }
    Shortcut { sequences: Keymap.sequences("reader.mode"); onActivated: reader.setOpt("readerMode", reader.mode === "strip" ? "paged" : "strip") }
    Shortcut { sequences: Keymap.sequences("reader.wider"); onActivated: reader.setOpt("readerWidth", Math.min(2400, reader.stripWidth + 80)) }
    Shortcut { sequences: Keymap.sequences("reader.narrower"); onActivated: reader.setOpt("readerWidth", Math.max(360, reader.stripWidth - 80)) }

    // ------------------------------------------------------------ strip

    ListView {
        id: strip
        anchors.fill: parent
        visible: reader.mode === "strip"
        interactive: !reader.magnifying
        transform: Scale {
            origin.x: reader.magnifyAt.x
            origin.y: reader.magnifyAt.y
            xScale: reader.magnifying ? 2 : 1
            yScale: xScale
            Behavior on xScale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
        }
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
            if (moving || stripWheel.ease.running) reader.controlsShown = false
        }

        // Wheel, touchpad and keys (scrollBy) share one eased scroll.
        WheelScroll {
            id: stripWheel
            wheelStep: 180
        }

        // Phone: a tap toggles the controls; on desktop single clicks are left
        // alone and a double click does. On the list itself, which takes
        // touches before anything behind it.
        TapHandler {
            onSingleTapped: (point) => {
                if (reader.stripTaps) {
                    const p = reader.mapFromItem(null, point.scenePosition.x, point.scenePosition.y)
                    reader.act(reader.tapZone(p.x, p.y, reader.width, reader.height))
                } else if (App.platform === "android") {
                    reader.toggleControls()
                }
            }
            onDoubleTapped: if (App.platform !== "android") reader.toggleControls()
            onLongPressed: reader.startMagnifier(strip, point.position.x, point.position.y)
        }
        // Follows the finger while magnifying, and lets go with it.
        PointHandler {
            onPointChanged: reader.moveMagnifier(point.position.x, point.position.y)
            onActiveChanged: if (!active) reader.magnifying = false
        }

        // Ctrl+wheel: the strip's width.
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.ControlModifier
            onWheel: (event) => reader.setOpt("readerWidth", Math.max(360, Math.min(2400, reader.stripWidth + (event.angleDelta.y > 0 ? 60 : -60))))
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
                filter: reader.imageFilter
                maxDecodeWidth: 2400
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
        transform: Scale {
            origin.x: reader.magnifyAt.x
            origin.y: reader.magnifyAt.y
            xScale: reader.magnifying ? 2 : 1
            yScale: xScale
            Behavior on xScale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
        }
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
        function isWide(i) {
            if (widePages[i] !== undefined) return widePages[i]
            const p = reader.panels[i]
            return !!p && p.width > 0 && p.height > 0 && p.width > p.height * 1.1
        }
        readonly property var views: {
            const out = []
            const n = reader.panels.length
            const wide = (i) => isWide(i)
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
        // A wide page split in two: which half is showing (0 the first in
        // reading order), and which side of the image that is.
        property int part: 0
        readonly property bool split: reader.splitWide && !twoUp && shown.length === 1 && isWide(shown[0])
        readonly property string half: split ? ((part === 0) !== rtl ? "left" : "right") : ""
        // Turning to one or two pages keeps the same first page in view.
        onTwoUpChanged: page = views[viewOf(page)][0]

        function turn(step) {
            if (split && (step > 0 ? part === 0 : part === 1)) {
                direction = step
                part = step > 0 ? 1 : 0
                pageSwap.restart()
                return
            }
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
            // Back onto a split page: its second half first.
            if (step < 0 && split) part = 1
        }
        onPageChanged: {
            part = 0
            resetZoom()
            // From `page` itself: `view` and `shown` may not have caught up
            // when this runs.
            const v = viewOf(page)
            const pagesShown = views[v] || [page]
            reader.currentPanel = pagesShown[pagesShown.length - 1]
            if (v === views.length - 1) reader.markFinished()
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
                        // Decoded for the width it is fitted to, again
                        // sharper for a zoom.
                        fitIn: Qt.size(pages.slot, pages.height)
                        zoom: zoomLayer.scale
                        readonly property real ratio: sourceWidth > 0 ? sourceHeight / sourceWidth : 1.45
                        width: Math.min(pages.slot, pages.height / ratio)
                        height: width * ratio
                        x: !pages.two ? (pages.width - width) / 2
                            : pageView.rtl ? pages.width / 2 : pages.width / 2 - width
                        y: (pages.height - height) / 2
                        source: reader.mode === "paged" && reader.panels.length > 0 ? reader.panels[Math.min(pageView.shown[0], reader.panels.length - 1)].url : ""
                        referer: reader.referer
                        cropBorders: reader.crop
                        filter: reader.imageFilter
                        half: pageView.half
                        maxDecodeWidth: 2400
                        // A half is half the page's width.
                        onLoaded: pageView.noteShape(pageView.shown[0], half ? sourceWidth * 2 : sourceWidth, sourceHeight)
                    }
                    PanelImage {
                        id: secondImage
                        // Decoded for the width it is fitted to, again
                        // sharper for a zoom.
                        fitIn: Qt.size(pages.slot, pages.height)
                        zoom: zoomLayer.scale
                        readonly property real ratio: sourceWidth > 0 ? sourceHeight / sourceWidth : 1.45
                        visible: pages.two
                        width: Math.min(pages.slot, pages.height / ratio)
                        height: width * ratio
                        x: pageView.rtl ? pages.width / 2 - width : pages.width / 2
                        y: (pages.height - height) / 2
                        source: pages.two && reader.mode === "paged" ? reader.panels[pageView.shown[1]].url : ""
                        referer: reader.referer
                        cropBorders: reader.crop
                        filter: reader.imageFilter
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
                // Fetched and measured only: the page shown decodes its own.
                preload: true
                source: modelData >= 0 && modelData < reader.panels.length ? reader.panels[modelData].url : ""
                referer: reader.referer
                cropBorders: reader.crop
                filter: reader.imageFilter
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
            // The tap zones turn the page (unless zoomed in) or show the
            // controls; a double click on the controls' zone zooms.
            onClicked: (mouse) => { if (!mouse.wasHeld) reader.act(pageView.zoomed ? "menu" : reader.tapZone(mouse.x, mouse.y, width, height)) }
            onDoubleClicked: (mouse) => {
                if (reader.doubleTapZoom && (pageView.zoomed || reader.tapZone(mouse.x, mouse.y, width, height) === "menu"))
                    pageView.toggleZoom(mouse.x, mouse.y)
            }
            onPressAndHold: (mouse) => { if (!pageView.zoomed && !swipe.active && !pinch.active) reader.startMagnifier(pageView, mouse.x, mouse.y) }
            onPositionChanged: (mouse) => reader.moveMagnifier(mouse.x, mouse.y)
            onReleased: reader.magnifying = false
            onCanceled: reader.magnifying = false
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
                enabled: !pageView.zoomed && !pinch.active && !reader.magnifying
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

    // Colour filter shades: warmth, then brightness. They take no input.
    Rectangle {
        anchors.fill: parent
        color: "#ff9a3c"
        opacity: reader.warmth * 0.28
        visible: opacity > 0
    }
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 1 - reader.brightness
        visible: opacity > 0
    }

    // The tap zones, shown for a moment when they change.
    Grid {
        id: zonePreview
        anchors.fill: parent
        z: 40
        columns: 3
        opacity: 0
        visible: opacity > 0
        Repeater {
            model: 9
            Rectangle {
                required property int index
                width: reader.width / 3
                height: reader.height / 3
                readonly property string action: reader.tapZone((index % 3 + 0.5) * width, (Math.floor(index / 3) + 0.5) * height, reader.width, reader.height)
                color: action === "next" ? Qt.rgba(0.2, 0.75, 0.45, 0.45) : action === "prev" ? Qt.rgba(0.95, 0.45, 0.3, 0.45) : Qt.rgba(1, 1, 1, 0.08)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.15)
                Text {
                    anchors.centerIn: parent
                    text: parent.action === "next" ? qsTr("Next") : parent.action === "prev" ? qsTr("Previous") : qsTr("Menu")
                    color: "white"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                }
            }
        }
        SequentialAnimation {
            id: zoneFlash
            NumberAnimation { target: zonePreview; property: "opacity"; to: 1; duration: Motion.fast }
            PauseAnimation { duration: 1400 }
            NumberAnimation { target: zonePreview; property: "opacity"; to: 0; duration: Motion.slow }
        }
    }
    onTapZonesChanged: zoneFlash.restart()
    onTapInvertChanged: zoneFlash.restart()

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
    // left edge: pages not read yet very light, read pages a heavier grey,
    // the page you are on black (each colour set in Settings > Reading). Tap
    // one to go to that page, or drag along it.
    function jumpTo(i) {
        if (i < 0 || i >= reader.panels.length) return
        if (reader.mode === "strip") strip.positionViewAtIndex(i, ListView.Beginning)
        else pageView.page = pageView.views[pageView.viewOf(i)][0]
    }
    Item {
        id: rail
        // Above the controls' shading, so it reads the same with them shown.
        z: 50
        // Full screen (or the "with controls" setting) keeps it out of the way
        // until the controls come up.
        readonly property var mode: App.settings.readerProgressBar
        readonly property bool shown: mode !== false && reader.panels.length > 0
                                      && (mode !== "controls" && !fullScreen.fullScreen || reader.controlsShown)
        readonly property int thickness: Number(App.settings.readerProgressWidth) || 3
        readonly property color unread: App.settings.readerRailUnread || "#e4e4e7"
        readonly property color read: App.settings.readerRailRead || "#8b8b94"
        readonly property color current: App.settings.readerRailCurrent || "#000000"
        opacity: shown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: reader.insetTop
        // Wider than what it paints, so the segments are easy to hit.
        width: thickness * 2 + 6
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
                width: rail.thickness
                height: Math.max(1, endY - startY - rail.gap)
                color: index === reader.currentPanel ? rail.current
                     : index < reader.currentPanel || reader.finished ? rail.read : rail.unread
                Behavior on color { ColorAnimation { duration: Motion.fast } }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            // The page moves with the finger; the strip does not take the drag.
            preventStealing: true
            function pick(y) {
                const i = Math.max(0, Math.min(rail.count - 1, Math.floor(y / (rail.segment + rail.gap))))
                if (i !== reader.currentPanel) reader.jumpTo(i)
                // Shown controls (and with them the bar) stay while it is used.
                if (reader.controlsShown) reader.showControls()
            }
            onPressed: (mouse) => pick(mouse.y)
            onPositionChanged: (mouse) => { if (pressed) pick(mouse.y) }
        }
    }

    // ------------------------------------------------------------ top bar

    // The series, and the chapter with the page you are on.
    component TitleLines: Column {
        property real maxWidth: 200
        spacing: 1
        Text {
            text: reader.series.title || ""
            color: "white"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
            font.weight: Font.DemiBold
            width: Math.max(0, Math.min(implicitWidth, parent.maxWidth))
            elide: Text.ElideRight
        }
        Text {
            text: (reader.unit.title || "")
                  + (reader.panels.length ? "  ·  " + (reader.currentPanel + 1) + " / " + reader.panels.length : "")
                  + (reader.task && reader.task.ok && reader.task.result.offline ? qsTr("  ·  Downloaded") : "")
                  + (App.settings.incognito === true ? qsTr("  ·  Incognito") : "")
            color: "#b4b4bb"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            width: Math.max(0, Math.min(implicitWidth, parent.maxWidth))
            elide: Text.ElideRight
        }
    }

    // Keeps the status bar legible over light pages while the controls are
    // hidden. Drawn over the top bar, which slides away under it instead of
    // across the phone's clock and icons.
    Rectangle {
        z: 3
        width: parent.width
        height: reader.insetTop
        visible: height > 0
        color: Qt.rgba(0, 0, 0, 0.6)
    }

    Item {
        id: topBar
        // Phones: right under the status bar, as in the novel reader.
        readonly property int pad: Theme.compact ? 8 : 16
        // Too narrow for the title beside the buttons.
        readonly property bool twoRows: reader.width - 100 - topActions.width < 160
        width: parent.width
        height: (Theme.compact ? 64 : 76) + (twoRows ? 56 : 0) + reader.insetTop
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
                GradientStop { position: (reader.insetTop + topBar.pad + 36 + (topBar.twoRows ? 40 : 0)) / topBar.height; color: Qt.rgba(0, 0, 0, 0.72) }
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
            TitleLines {
                visible: !topBar.twoRows
                anchors.verticalCenter: parent.verticalCenter
                maxWidth: reader.width - 100 - topActions.width
            }
        }
        // Phones: the buttons take the first row, the title the second.
        TitleLines {
            visible: topBar.twoRows
            x: 18
            y: topBar.pad + reader.insetTop + 40
            maxWidth: reader.width - 36
        }
        Row {
            id: topActions
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: topBar.pad + reader.insetTop
            // Phones: smaller buttons, no gaps, so all fit beside the close button.
            readonly property int button: reader.width < 420 ? 32 : 34
            spacing: reader.width < 420 ? 0 : 4
            // Pages are on the progress bar at the left edge; chapters here.
            IconButton {
                diameter: topActions.button
                iconName: "skip-back"
                iconColor: "white"
                enabled: reader.hasPrev
                opacity: enabled ? 1 : 0.4
                tip: qsTr("Previous chapter") + reader.keyHint("reader.prev")
                onClicked: reader.load(reader.index - 1)
            }
            IconButton {
                diameter: topActions.button
                iconName: "skip-forward"
                iconColor: "white"
                enabled: reader.hasNext
                opacity: enabled ? 1 : 0.4
                tip: qsTr("Next chapter") + reader.keyHint("reader.next")
                onClicked: reader.load(reader.index + 1)
            }
            IconButton {
                diameter: topActions.button
                iconName: "list"
                iconColor: "white"
                tip: qsTr("Chapters")
                onClicked: chapterPopup.open()
            }
            IconButton {
                diameter: topActions.button
                iconName: reader.mode === "strip" ? "move-vertical" : "layers"
                iconColor: "white"
                tip: (reader.mode === "strip" ? qsTr("Switch to pages") : qsTr("Switch to vertical strip")) + reader.keyHint("reader.mode")
                onClicked: reader.setOpt("readerMode", reader.mode === "strip" ? "paged" : "strip")
            }
            IconButton {
                diameter: topActions.button
                visible: !String(reader.series.sourceUrl || "").startsWith("file:")
                iconName: "message-circle"
                iconColor: "white"
                tip: qsTr("Comments")
                onClicked: comments.open()
            }
            ContinueOn {
                diameter: topActions.button
                seriesId: reader.series.id || ""
                iconColor: "white"
                save: () => reader.saveProgress(true)
            }
            IconButton {
                diameter: topActions.button
                iconName: "sliders-horizontal"
                iconColor: "white"
                tip: qsTr("Reader settings")
                onClicked: settingsPopup.open()
            }
            IconButton {
                diameter: topActions.button
                iconName: fullScreen.fullScreen ? "minimize" : "maximize"
                iconColor: "white"
                tip: qsTr("Full screen") + reader.keyHint("reader.fullscreen")
                onClicked: fullScreen.toggle()
            }
        }
    }

    // ------------------------------------------------------------ popups

    Popup {
        id: chapterPopup
        x: parent.width - width - 18
        y: topBar.height - 6
        width: Math.min(340, parent.width - 36)
        height: Math.min(parent.height - 160 - reader.insetTop - reader.insetBottom, 560)
        padding: 6
        onOpened: chapterList.positionViewAtIndex(reader.index, ListView.Center)
        background: LiquidGlass {
            nativeEnabled: false
            radius: Theme.radiusLg
            tintColor: Qt.rgba(0.08, 0.08, 0.10, 0.70)
        }
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
                    color: index === reader.index ? Qt.rgba(1, 1, 1, 0.14) : parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
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
        y: topBar.height - 6
        width: Math.min(320, parent.width - 36)
        padding: 18
        background: LiquidGlass {
            nativeEnabled: false
            radius: Theme.radiusLg
            tintColor: Qt.rgba(0.08, 0.08, 0.10, 0.70)
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast }
                NumberAnimation { property: "y"; from: topBar.height - 16; to: topBar.height - 6; duration: Motion.normal; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }
        // Scrolls when it is taller than the window (phones).
        contentItem: Flickable {
            implicitHeight: Math.min(settingsColumn.implicitHeight, reader.height - 110 - reader.insetTop - reader.insetBottom)
            contentHeight: settingsColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}
            Column {
                id: settingsColumn
                width: parent.width
                spacing: 16
                component Heading: Text { color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                component Label: Text { anchors.verticalCenter: parent.verticalCenter; color: "#d4d4d8"; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                Heading { text: qsTr("Layout") }
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
                    Label { text: qsTr("Trim white or black borders") }
                }
                Row {
                    visible: reader.mode === "paged"
                    spacing: 10
                    Toggle {
                        checked: reader.splitWide
                        onToggled: reader.setOpt("readerSplitWide", checked)
                    }
                    Label { text: qsTr("Split wide pages in two") }
                }
                Heading { visible: reader.mode === "strip"; text: qsTr("Width · %1 px").arg(reader.stripWidth) }
                ScSlider {
                    visible: reader.mode === "strip"
                    width: parent.width
                    from: 360
                    to: 2000
                    stepSize: 20
                    value: reader.stripWidth
                    onMoved: reader.setOpt("readerWidth", value)
                }
                Heading { visible: reader.mode === "strip"; text: qsTr("Gap between panels · %1 px").arg(reader.gap) }
                ScSlider {
                    visible: reader.mode === "strip"
                    width: parent.width
                    from: 0
                    to: 40
                    stepSize: 2
                    value: reader.gap
                    onMoved: reader.setOpt("readerGap", value)
                }

                Rectangle { width: parent.width; height: 1; color: "#2a2a30" }
                Heading { text: qsTr("Colour · brightness %1%").arg(Math.round(reader.brightness * 100)) }
                ScSlider {
                    width: parent.width
                    from: 0.2
                    to: 1
                    stepSize: 0.05
                    value: reader.brightness
                    onMoved: reader.setOpt("readerBrightness", value)
                }
                Heading { text: qsTr("Warmth · %1%").arg(Math.round(reader.warmth * 100)) }
                ScSlider {
                    width: parent.width
                    from: 0
                    to: 1
                    stepSize: 0.05
                    value: reader.warmth
                    onMoved: reader.setOpt("readerWarmth", value)
                }
                Row {
                    spacing: 10
                    Toggle {
                        checked: reader.opt("readerGrey", false) === true
                        onToggled: reader.setOpt("readerGrey", checked)
                    }
                    Label { text: qsTr("Greyscale") }
                }
                Row {
                    spacing: 10
                    Toggle {
                        checked: reader.opt("readerInvert", false) === true
                        onToggled: reader.setOpt("readerInvert", checked)
                    }
                    Label { text: qsTr("Invert colours") }
                }

                Rectangle { width: parent.width; height: 1; color: "#2a2a30" }
                Heading { text: qsTr("Taps") }
                Select {
                    width: parent.width
                    Accessible.name: qsTr("Tap zones")
                    model: reader.tapZoneOptions
                    value: reader.tapZones
                    onPicked: (v) => reader.setOpt("readerTapZones", v)
                }
                Select {
                    width: parent.width
                    visible: reader.tapZones !== "off"
                    Accessible.name: qsTr("Flip the tap zones")
                    model: reader.tapInvertOptions
                    value: reader.tapInvert
                    onPicked: (v) => reader.setOpt("readerTapInvert", v)
                }
                Row {
                    visible: reader.mode === "strip" && reader.tapZones !== "off"
                    spacing: 10
                    Toggle {
                        checked: reader.stripTaps
                        onToggled: reader.setOpt("readerStripTaps", checked)
                    }
                    Label { text: qsTr("Tap zones in the vertical strip") }
                }
                Row {
                    visible: App.platform === "android"
                    spacing: 10
                    Toggle {
                        checked: reader.volumeKeys
                        onToggled: reader.setOpt("readerVolumeKeys", checked)
                    }
                    Label { text: qsTr("Volume keys turn pages") }
                }

                Rectangle { width: parent.width; height: 1; color: "#2a2a30" }
                Heading { text: qsTr("Page progress bar") }
                Segmented {
                    options: [{ value: true, label: qsTr("Always") }, { value: "controls", label: qsTr("With controls") }, { value: false, label: qsTr("Off") }]
                    value: rail.mode
                    onPicked: (v) => App.setSetting("readerProgressBar", v)
                }
                Segmented {
                    visible: rail.mode !== false
                    options: [{ value: 3, label: qsTr("Thin") }, { value: 6, label: qsTr("Medium") }, { value: 10, label: qsTr("Thick") }]
                    value: rail.thickness
                    onPicked: (v) => App.setSetting("readerProgressWidth", v)
                }
                Row {
                    spacing: 10
                    Toggle {
                        checked: fullScreen.fullScreen
                        onToggled: if (checked !== fullScreen.fullScreen) fullScreen.toggle()
                    }
                    Label { text: qsTr("Full screen") }
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
                        Text { text: qsTr("Only for this series"); color: "#d4d4d8"; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                        Text { text: qsTr("Keeps these settings for %1 alone").arg(reader.series.title || qsTr("it")); width: 230; elide: Text.ElideRight; color: "#8a8a92"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs + 1 }
                    }
                }
            }
        }
    }

    Rectangle {
        id: einkFlashCover
        anchors.fill: parent
        z: 1000
        color: "black"
        visible: einkFlash.running
    }
    Timer { id: einkFlash; interval: 120 }
}
