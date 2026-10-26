import QtQuick
import Scancore

// A short sample chapter for the welcome: six panels of our own art, read
// as a strip (scroll) or as pages (tap the sides), with the reader's real
// progress bar on it in the look being picked.
Rectangle {
    id: sample
    readonly property bool paged: App.settings.readerMode === "paged"
    readonly property int count: lines.length
    property int current: 0
    readonly property var lines: [
        qsTr("Scroll down, or tap a side to turn the page."),
        qsTr("The bar shows how far into the chapter you are."),
        qsTr("Tap or drag the bar to jump to a page."),
        qsTr("Pick another look for it and see it here."),
        qsTr("Pages or one long strip: switch above."),
        qsTr("The end. Your real chapters start right after this.")]
    color: "#0c0c0d"
    radius: Theme.retro ? 0 : 12
    clip: true
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Sample chapter")

    function jumpTo(i) {
        current = Math.max(0, Math.min(count - 1, i))
        if (!paged) strip.contentY = Math.min(strip.contentHeight - strip.height, current * panelHeight)
    }
    readonly property real panelHeight: Math.round(width * 1.25)

    // One panel: sky, a sun or moon, hills, a speech bubble.
    component Panel: Item {
        id: panel
        property int page: 0
        readonly property var hue: [[0.58, 0.66], [0.07, 0.95], [0.75, 0.62], [0.45, 0.55], [0.98, 0.08], [0.62, 0.70]][page % 6]
        clip: true
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.hsla(panel.hue[0], 0.55, 0.62, 1) }
                GradientStop { position: 1; color: Qt.hsla(panel.hue[1], 0.6, 0.22, 1) }
            }
        }
        Rectangle {
            width: panel.width * 0.34; height: width; radius: width / 2
            x: panel.width * [0.6, 0.1, 0.55, 0.2, 0.65, 0.4][panel.page % 6]
            y: panel.height * 0.14
            color: Qt.hsla(panel.hue[0], 0.8, 0.85, 0.85)
        }
        Repeater {
            model: 3
            Rectangle {
                required property int index
                width: panel.width * 1.4
                height: panel.height * 0.5
                x: -panel.width * 0.2 + index * panel.width * 0.25
                y: panel.height * (0.62 + index * 0.08)
                rotation: [-12, 9, -5][index]
                color: Qt.hsla(panel.hue[1], 0.45, 0.12 + index * 0.04, 1)
            }
        }
        // Speech bubble.
        Rectangle {
            x: panel.width * 0.08
            y: panel.height * (panel.page % 2 ? 0.48 : 0.36)
            width: panel.width * 0.84
            height: bubbleText.implicitHeight + 24
            radius: height / 2.4
            color: "#ffffff"
            border.width: 2
            border.color: "#111111"
            Text {
                id: bubbleText
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                text: sample.lines[panel.page]
                color: "#111111"
                font.family: Theme.fontDisplay
                font.pixelSize: Math.max(11, panel.width * 0.05)
                font.weight: Font.Bold
                wrapMode: Text.Wrap
            }
        }
        Text {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            text: (panel.page + 1) + " / " + sample.count
            color: Qt.rgba(1, 1, 1, 0.6)
            font.family: Theme.fontSans
            font.pixelSize: 10
        }
    }

    // As a strip.
    Flickable {
        id: strip
        visible: !sample.paged
        anchors.fill: parent
        contentHeight: sample.count * sample.panelHeight
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: Motion.flickVelocity
        flickDeceleration: Motion.flickDeceleration
        WheelScroll {}
        onContentYChanged: if (!sample.paged) sample.current = Math.min(sample.count - 1, Math.floor((contentY + height / 2) / sample.panelHeight))
        Column {
            Repeater {
                model: sample.count
                Panel {
                    required property int index
                    page: index
                    width: sample.width
                    height: sample.panelHeight
                }
            }
        }
    }

    // As pages: the left and right thirds turn.
    Panel {
        visible: sample.paged
        anchors.fill: parent
        page: sample.current
    }
    TapHandler {
        enabled: sample.paged
        onTapped: (point) => {
            if (point.position.x > sample.width * 0.6) sample.jumpTo(sample.current + 1)
            else if (point.position.x < sample.width * 0.4) sample.jumpTo(sample.current - 1)
        }
    }

    // The reader's bar, in the look and colours being picked.
    ReaderProgress {
        anchors.fill: parent
        visible: App.settings.readerProgressBar !== false
        count: sample.count
        current: sample.current
        finished: false
        look: App.settings.readerProgressLook || "segments"
        edge: App.settings.readerProgressEdge || "left"
        thickness: Number(App.settings.readerProgressWidth) || 3
        unread: App.settings.readerRailUnread || "#e4e4e7"
        read: App.settings.readerRailRead || "#8b8b94"
        currentColor: App.settings.readerRailCurrent || "#000000"
        strength: App.settings.readerProgressOpacity !== undefined ? Number(App.settings.readerProgressOpacity) : 0.85
        onPicked: (i) => sample.jumpTo(i)
    }
}
