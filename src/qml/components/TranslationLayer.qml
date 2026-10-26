import QtQuick
import QtQuick.Controls.Basic
import Scancore

// A comic page's translation, over the page: each bubble the translator
// found is covered with its translation, fitted to the bubble. A tap on one
// shows the original under it for a moment. Fill it over the page item.
Item {
    id: overlay
    property string url
    property string referer
    property string from
    property bool translating: false
    // What helps a language model translate as one story (Translator.page's
    // hint): the series, the chapter, and the pages around this one.
    property var context: ({})
    // A page shown as two halves (paged reading): only this half's
    // bubbles, spread over the whole width.
    property string half: ""

    property var task: null
    readonly property var blocks: {
        const all = task && task.ok ? Array.from(task.result.blocks) : []
        if (!half) return all
        const from = half === "right" ? 0.5 : 0
        return all.filter(b => b.x + b.w / 2 >= from && b.x + b.w / 2 < from + 0.5)
                  .map(b => Object.assign({}, b, { x: (b.x - from) * 2, w: b.w * 2 }))
    }
    onTranslatingChanged: refresh()
    onUrlChanged: refresh()
    onFromChanged: refresh()
    // Another language asked for in the reader's bar.
    readonly property string target: Translator.target
    onTargetChanged: refresh()
    Component.onCompleted: refresh()
    // Once per page and language (and target), however many of these change on the way.
    property string asked: ""
    function refresh() { Qt.callLater(ask) }
    function ask() {
        const key = translating && url ? url + "|" + from + "|" + target : ""
        if (key === asked) return
        asked = key
        task = key ? Translator.page(url, referer, from, context || {}) : null
    }

    visible: translating

    Repeater {
        model: overlay.blocks
        delegate: Item {
            id: bubble
            required property var modelData
            property bool peek: false
            // A little more than the text's own box, so the original's
            // edges are covered too.
            readonly property real pad: Math.max(2, overlay.width * 0.006)
            x: modelData.x * overlay.width - pad
            y: modelData.y * overlay.height - pad
            width: modelData.w * overlay.width + 2 * pad
            height: modelData.h * overlay.height + 2 * pad

            Rectangle {
                anchors.fill: parent
                radius: Math.min(width, height) * 0.25
                color: "#fbfbfb"
                opacity: bubble.peek ? 0 : 0.97
                Behavior on opacity { NumberAnimation { duration: Motion.fast } }
            }
            Text {
                anchors.fill: parent
                anchors.margins: bubble.pad
                visible: !bubble.peek
                text: bubble.modelData.text
                color: "#111111"
                font.family: Theme.fontSans
                font.weight: Font.DemiBold
                font.pixelSize: Math.max(10, Math.min(64, parent.height * 0.8))
                fontSizeMode: Text.Fit
                minimumPixelSize: 6
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                lineHeight: 0.95
            }
            TapHandler {
                onTapped: {
                    bubble.peek = true
                    peekTimer.restart()
                }
            }
            Timer { id: peekTimer; interval: 1800; onTriggered: bubble.peek = false }
        }
    }

    // Working on it, or why not, in the page's corner.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        visible: overlay.task !== null && (overlay.task.loading || !overlay.task.ok)
        width: Math.min(parent.width - 16, status.implicitWidth + 20)
        height: status.implicitHeight + 12
        radius: Theme.pill(height)
        color: Qt.rgba(0, 0, 0, 0.72)
        Text {
            id: status
            anchors.centerIn: parent
            width: Math.min(implicitWidth, overlay.width - 36)
            text: overlay.task && overlay.task.loading ? qsTr("Translating…") : overlay.task ? overlay.task.error : ""
            color: "white"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            wrapMode: Text.Wrap
        }
        TapHandler {
            enabled: overlay.task !== null && !overlay.task.loading
            onTapped: {
                overlay.asked = ""
                overlay.refresh()
            }
        }
    }
}
