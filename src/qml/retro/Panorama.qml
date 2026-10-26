import QtQuick
import Scancore

// The Retro hub: one long canvas of sections under a huge title, panned
// sideways. Its layers move at their own speeds (Windows Phone's
// panorama): the background slower than the sections, the title slowest.
// On phones the next section peeks in at the right and the hub wraps
// around, the last section leading back to the first; on wide windows the
// sections sit side by side, as on a start screen, and scroll with the
// wheel.
//   sections: [{ key, label }], pages: { key: Component }.
// A page gets `sectionKey` and `current` (it is the section in view).
FocusScope {
    id: pano
    property string title: "scancore"
    property bool gradientTitle: true
    property string background: ""
    property string backgroundReferer: ""
    property var sections: []
    property var pages: ({})
    property bool upperCase: true
    // Phones in portrait: one section at a time, wrapping around.
    readonly property bool cyclic: Theme.compact && sections.length >= 3

    readonly property int count: sections.length
    property int currentIndex: 0
    readonly property string currentKey: count > 0 ? sections[Math.max(0, Math.min(count - 1, currentIndex))].key : ""

    signal headerClicked(string key)

    readonly property int margin: Theme.compact ? 16 : 28
    readonly property real peek: cyclic ? Math.round(width * 0.11) : 0
    readonly property real sectionWidth: cyclic ? width - peek
        : Math.round(Math.max(340, Math.min(560, width * 0.36)))
    readonly property real titleSize: Theme.compact ? Math.max(64, Math.min(132, width * 0.24))
        : Math.max(96, Math.min(168, height * 0.17))
    readonly property real headerSize: Theme.compact ? Math.max(18, Math.min(26, width * 0.055)) : 24
    readonly property real contentTop: titleSize * 1.02 + headerSize + 22

    // How far along the canvas the view is, 0 to 1 and, wrapping, back.
    readonly property real shift: {
        if (cyclic) {
            const pos = ((strip.pos % count) + count) % count
            return count <= 1 ? 0 : pos <= count - 1 ? pos / (count - 1) : count - pos
        }
        const room = row.contentWidth - row.width
        return room > 1 ? Math.max(0, Math.min(1, row.contentX / room)) : 0
    }

    function go(index) {
        const i = ((index % count) + count) % count
        if (cyclic) strip.settle(i, false)
        else row.positionViewAtIndex(i, ListView.Beginning)
        currentIndex = i
    }
    function goKey(key) {
        const i = sections.findIndex(s => s.key === key)
        if (i >= 0) go(i)
    }
    function next() { cyclic ? strip.settle(Math.round(strip.pos) + 1, true) : go(Math.min(count - 1, currentIndex + 1)) }
    function previous() { cyclic ? strip.settle(Math.round(strip.pos) - 1, true) : go(Math.max(0, currentIndex - 1)) }

    Keys.onLeftPressed: previous()
    Keys.onRightPressed: next()

    clip: true

    // ---------------------------------------------------------- background
    Item {
        id: backdrop
        readonly property real travel: Math.round(pano.width * (pano.cyclic ? 0.35 : 0.2))
        anchors.fill: parent
        Image {
            id: picture
            x: -pano.shift * backdrop.travel
            width: pano.width + backdrop.travel
            height: pano.height
            source: pano.background ? App.image(pano.background, pano.backgroundReferer) : ""
            // A cover is small: decode it near the size it is drawn, once.
            sourceSize.width: Math.min(1280, Math.ceil(width * Screen.devicePixelRatio * 0.6))
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Motion.slow * 2 } }
        }
        // Dark, soft and low-contrast, as the guidelines asked, so the text
        // over it never depends on the picture.
        Rectangle {
            anchors.fill: parent
            visible: picture.status === Image.Ready
            color: Theme.alpha(Theme.base, Theme.dark ? 0.68 : 0.8)
        }
    }

    // ---------------------------------------------------------- title
    Item {
        id: titleLayer
        readonly property real travel: Math.max(titleText.implicitWidth * 0.45,
                                                titleText.implicitWidth - pano.width + 2 * pano.margin)
        x: pano.margin - 4 - pano.shift * travel
        y: -pano.titleSize * 0.2
        width: titleText.implicitWidth
        height: titleText.implicitHeight

        Text {
            id: titleText
            text: pano.title
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: pano.titleSize
            font.weight: Font.Light
            font.letterSpacing: -pano.titleSize * 0.02
            // Shown as it is, or as the mask for the gradient below.
            visible: !pano.gradientTitle
            layer.enabled: pano.gradientTitle
        }
        // The gradient: the text's own layer gives the shape.
        ShaderEffect {
            anchors.fill: parent
            visible: pano.gradientTitle
            property var source: titleText
            property color from: Theme.tile
            property color to: Qt.hsla((Math.max(0, Theme.tile.hslHue) + 0.1) % 1, Math.max(0.45, Theme.tile.hslSaturation),
                                       Math.max(0.4, Theme.tile.hslLightness), 1)
            fragmentShader: "qrc:/shaders/title.frag.qsb"
        }
    }

    // ---------------------------------------------------------- sections
    component Section: Item {
        id: section
        required property var modelData
        required property int index
        readonly property bool current: index === pano.currentIndex
        width: pano.sectionWidth
        height: pano.height

        Text {
            id: header
            x: pano.margin
            y: pano.contentTop - pano.headerSize - 14
            width: parent.width - pano.margin - 12
            text: section.modelData.label
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: pano.headerSize
            font.weight: Font.DemiBold
            font.capitalization: pano.upperCase ? Font.AllUppercase : Font.AllLowercase
            font.letterSpacing: pano.upperCase ? 0.4 : 0
            elide: Text.ElideRight
            // The next section's header shows dimmed as it peeks in.
            opacity: section.current || !pano.cyclic ? 1 : 0.55
            Behavior on opacity { NumberAnimation { duration: Motion.normal } }
            TapHandler { onTapped: pano.headerClicked(section.modelData.key) }
        }
        Loader {
            x: pano.margin - 4
            y: pano.contentTop
            width: parent.width - x - (pano.cyclic ? 4 : 12)
            height: parent.height - y
            // The first section in view comes at once; the others follow
            // over the next frames.
            asynchronous: !section.current
            sourceComponent: pano.pages[section.modelData.key] || null
            onLoaded: {
                item.sectionKey = Qt.binding(() => section.modelData.key)
                item.current = Qt.binding(() => section.current)
            }
        }
    }

    // Phones: the sections side by side on a loop. `pos` is where the view
    // is in sections (2.5 = half way from the third to the fourth); each
    // section sits at its distance from it, wrapped so the one before the
    // current waits off the left and the rest follow on the right. A
    // DragHandler pans it: it takes a sideways drag over a tile or a list
    // row and leaves taps and vertical scrolling to them (PathView lost the
    // finger's release when a tile had taken the press, and kept dragging).
    Item {
        id: strip
        anchors.fill: parent
        visible: pano.cyclic
        enabled: visible
        property real pos: 0
        // Where the finger started, and how fast it was going.
        property real startPos: 0
        property real lastX: 0
        property real velocity: 0
        property double lastTime: 0

        function wrapped(i) { return ((i % pano.count) + pano.count) % pano.count }
        function settle(target, animated) {
            ease.stop()
            if (!animated || Motion.reduced) {
                pos = wrapped(target)
                pano.currentIndex = wrapped(target)
                return
            }
            ease.from = pos
            ease.to = target
            ease.restart()
        }
        NumberAnimation {
            id: ease
            target: strip
            property: "pos"
            duration: Motion.normal
            easing.type: Easing.OutCubic
            onFinished: {
                strip.pos = strip.wrapped(Math.round(strip.pos))
                pano.currentIndex = strip.wrapped(Math.round(strip.pos))
            }
        }
        // The section in view changes once the hub settles, not half way
        // through a pan: what follows it (the app bar's button, the saved
        // section) would stall the pan's frames.

        Repeater {
            model: pano.cyclic ? pano.sections : []
            delegate: Section {
                id: placed
                // Distance from the view, in sections: one before the
                // current goes to -1, the rest after it.
                readonly property real distance: {
                    let d = ((index - strip.pos) % pano.count + pano.count) % pano.count
                    if (d > pano.count - 1.5) d -= pano.count
                    return d
                }
                x: distance * pano.sectionWidth
                // Off screen ones need not be drawn.
                visible: distance > -1 && distance < pano.width / pano.sectionWidth + 0.01
            }
        }

        DragHandler {
            id: pan
            target: null
            enabled: pano.count > 1
            xAxis.enabled: true
            yAxis.enabled: false
            onActiveChanged: {
                if (active) {
                    ease.stop()
                    strip.startPos = strip.pos
                    strip.lastX = 0
                    strip.velocity = 0
                    strip.lastTime = Date.now()
                    return
                }
                // On release translation already reads 0: use what was
                // tracked while the finger moved.
                // A finger that stopped before lifting is not a flick.
                const v = Date.now() - strip.lastTime > 120 ? 0 : strip.velocity
                const moved = strip.pos - strip.startPos
                const from = Math.round(strip.startPos)
                // A quarter of a section or a light flick turns to the
                // next one; never more than one per swipe.
                let target = from
                if (moved > 0.25 || v < -150) target = from + 1
                else if (moved < -0.25 || v > 150) target = from - 1
                if ((target > from && v > 150) || (target < from && v < -150)) target = from
                strip.settle(target, true)
            }
            onTranslationChanged: {
                if (!active) return
                const now = Date.now()
                const dt = Math.max(1, now - strip.lastTime)
                const dx = translation.x - strip.lastX
                // Pixels a second, smoothed.
                strip.velocity = strip.velocity * 0.4 + (dx / dt * 1000) * 0.6
                strip.lastX = translation.x
                strip.lastTime = now
                strip.pos = strip.startPos - translation.x / pano.sectionWidth
            }
        }
    }

    ListView {
        id: row
        anchors.fill: parent
        visible: !pano.cyclic
        enabled: visible
        orientation: ListView.Horizontal
        model: pano.cyclic ? [] : pano.sections
        leftMargin: Theme.compact ? 0 : pano.margin
        rightMargin: pano.margin
        spacing: Theme.compact ? 0 : 24
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: Motion.flickVelocity
        flickDeceleration: Motion.flickDeceleration
        // Phones in landscape and narrow windows: one section at a time.
        snapMode: Theme.compact ? ListView.SnapOneItem : ListView.NoSnap
        highlightRangeMode: Theme.compact ? ListView.StrictlyEnforceRange : ListView.NoHighlightRange
        cacheBuffer: pano.sectionWidth * 2
        onContentXChanged: if (!pano.cyclic) pano.currentIndex = Math.max(0, Math.min(pano.count - 1,
            Math.round((contentX + leftMargin) / (pano.sectionWidth + spacing))))
        delegate: Section {}
    }

    // The wheel over the title, the headers or anything that does not
    // scroll itself pans the hub sideways.
    WheelHandler {
        enabled: !pano.cyclic
        target: null
        onWheel: (event) => {
            const d = event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y
            const room = Math.max(0, row.contentWidth - row.width)
            row.contentX = Math.max(-row.leftMargin, Math.min(room - row.leftMargin + row.rightMargin, row.contentX - d * 1.5))
        }
    }
}
