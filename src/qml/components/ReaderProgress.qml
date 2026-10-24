import QtQuick
import ScanCore

// How far through a chapter: the comic reader's progress bar, and its
// previews in Settings → Reading and the welcome screens. Three looks:
//   "segments": one flat piece per page, flush with the edge (tap a page)
//   "line":     one continuous line that fills as you read
//   "floating": a rounded bar off the edge, with the page number
// on the left, right or bottom edge. Drawn with a handful of rectangles
// (segments: one per page, reused), no layers or effects, so it costs
// nothing while scrolling.
Item {
    id: bar
    property int count: 0
    property int current: 0
    property bool finished: false
    property string look: "segments"
    property string edge: "left"
    // Right-to-left reading fills a bottom bar from the right.
    property bool reversed: false
    property int thickness: 3
    property color unread: "#e4e4e7"
    property color read: "#8b8b94"
    property color currentColor: "#000000"
    // How see-through the whole bar is (1 = solid).
    property real strength: 0.85
    property bool interactive: true
    // Free space between the bar and the window's edge (status bar).
    property real inset: 0
    property real insetBottom: 0
    // A page tapped or dragged to.
    signal picked(int index)

    readonly property bool horizontal: edge === "bottom"
    readonly property bool floating: look === "floating"
    // Along the bar, and across it (the part that is drawn).
    readonly property real length: horizontal ? width - (floating ? 2 * margin : 0) : height - inset - insetBottom - (floating ? 2 * margin : 0)
    readonly property real margin: floating ? Math.max(24, (horizontal ? width : height) * 0.2) : 0
    readonly property real drawn: floating ? Math.max(6, thickness + 3) : thickness
    readonly property real done: count > 0 ? (finished ? 1 : (current + 1) / count) : 0

    opacity: strength

    // The bar's box in the item: an edge strip, or a floating pill.
    Item {
        id: track
        readonly property real across: bar.drawn
        readonly property real off: bar.floating ? (bar.horizontal ? 18 : 10) : 0
        x: bar.horizontal ? bar.margin : bar.edge === "right" ? bar.width - across - off : off
        y: bar.horizontal ? bar.height - bar.insetBottom - across - off : bar.inset + bar.margin
        width: bar.horizontal ? bar.length : across
        height: bar.horizontal ? across : bar.length

        // ------------------------------------------------ segments
        Repeater {
            model: bar.look === "segments" ? bar.count : 0
            Rectangle {
                required property int index
                readonly property real gap: bar.count > 0 && (bar.length - (bar.count - 1)) / bar.count >= 3 ? 1 : 0
                readonly property real segment: bar.count > 0 ? (bar.length - (bar.count - 1) * gap) / bar.count : 0
                readonly property int at: bar.reversed && bar.horizontal ? bar.count - 1 - index : index
                // Whole pixels, so the 1 px gaps stay visible.
                readonly property int start: Math.round(at * (segment + gap))
                readonly property int end: Math.round((at + 1) * (segment + gap))
                x: bar.horizontal ? start : 0
                y: bar.horizontal ? 0 : start
                width: bar.horizontal ? Math.max(1, end - start - gap) : track.width
                height: bar.horizontal ? track.height : Math.max(1, end - start - gap)
                color: index === bar.current && !bar.finished ? bar.currentColor
                     : index < bar.current || bar.finished ? bar.read : bar.unread
            }
        }

        // ------------------------------------------------ line and floating
        Rectangle {
            visible: bar.look !== "segments"
            anchors.fill: parent
            radius: bar.floating ? width / 2 > height / 2 ? height / 2 : width / 2 : 0
            color: bar.unread
            Rectangle {
                readonly property real fill: Math.max(bar.floating ? Math.min(parent.width, parent.height) : 0,
                                                      (bar.horizontal ? parent.width : parent.height) * bar.done)
                x: bar.horizontal && bar.reversed ? parent.width - width : 0
                width: bar.horizontal ? fill : parent.width
                height: bar.horizontal ? parent.height : fill
                radius: parent.radius
                color: bar.read
                Behavior on width { enabled: bar.horizontal; NumberAnimation { duration: Motion.fast } }
                Behavior on height { enabled: !bar.horizontal; NumberAnimation { duration: Motion.fast } }
                // Where you are: the end of the fill.
                Rectangle {
                    visible: !bar.finished
                    readonly property real size: Math.max(bar.drawn, 4)
                    width: bar.horizontal ? Math.min(size, parent.width) : parent.width
                    height: bar.horizontal ? parent.height : Math.min(size, parent.height)
                    x: bar.horizontal ? (bar.reversed ? 0 : parent.width - width) : 0
                    y: bar.horizontal ? 0 : parent.height - height
                    radius: parent.radius
                    color: bar.currentColor
                }
            }
        }
    }

    // The floating bar's page number, beside it.
    Text {
        visible: bar.floating && bar.count > 0
        text: (bar.current + 1) + " / " + bar.count
        color: "white"
        style: Text.Outline
        styleColor: Qt.rgba(0, 0, 0, 0.6)
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs
        font.weight: Font.DemiBold
        font.features: { "tnum": 1 }
        x: bar.horizontal ? (bar.width - implicitWidth) / 2 : bar.edge === "right" ? track.x - implicitWidth - 6 : track.x + track.width + 6
        y: bar.horizontal ? track.y - implicitHeight - 4 : track.y + track.height * bar.done - implicitHeight / 2
    }

    MouseArea {
        enabled: bar.interactive && bar.count > 0
        // Wider than what it paints, so it is easy to hit.
        readonly property real reach: track.across + track.off + bar.thickness + 6
        x: bar.horizontal ? track.x : bar.edge === "right" ? bar.width - reach : 0
        y: bar.horizontal ? bar.height - bar.insetBottom - reach : track.y
        width: bar.horizontal ? track.width : reach
        height: bar.horizontal ? reach : track.height
        cursorShape: Qt.PointingHandCursor
        // The page moves with the finger; the strip does not take the drag.
        preventStealing: true
        function pick(pos) {
            const along = bar.horizontal ? (bar.reversed ? track.width - pos : pos) : pos
            bar.picked(Math.max(0, Math.min(bar.count - 1, Math.floor(along / track[bar.horizontal ? "width" : "height"] * bar.count))))
        }
        onPressed: (mouse) => pick(bar.horizontal ? mouse.x : mouse.y)
        onPositionChanged: (mouse) => { if (pressed) pick(bar.horizontal ? mouse.x : mouse.y) }
    }
}
