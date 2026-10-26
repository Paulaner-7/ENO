import QtQuick
import QtQuick.Controls.Basic
import Scancore

// The comic reader's progress bar, picked by looking at it: a small page
// for each look (with the colours and strength set now), and None. Used in
// Settings → Reading and on the welcome screens.
Flow {
    id: picker
    property real tileWidth: 112
    readonly property real tileHeight: Math.round(tileWidth * (Theme.compact ? 1.12 : 1.35))
    readonly property bool off: App.settings.readerProgressBar === false
    readonly property string look: App.settings.readerProgressLook || "segments"
    spacing: 10

    readonly property var choices: [
        { key: "segments", label: qsTr("Pages"), edge: "left" },
        { key: "line", label: qsTr("Line"), edge: "bottom" },
        { key: "floating", label: qsTr("Floating"), edge: "bottom" },
        { key: "none", label: qsTr("None") }]

    function choose(c) {
        if (c.key === "none") {
            App.setSetting("readerProgressBar", false)
            return
        }
        if (picker.off) App.setSetting("readerProgressBar", true)
        App.setSetting("readerProgressLook", c.key)
        // Its natural edge the first time; a chosen edge stays after that.
        if (picker.look !== c.key) App.setSetting("readerProgressEdge", c.edge)
    }

    Repeater {
        model: picker.choices
        AbstractButton {
            id: tile
            required property var modelData
            readonly property bool selected: modelData.key === "none" ? picker.off : !picker.off && picker.look === modelData.key
            width: picker.tileWidth
            height: picker.tileHeight + 26
            hoverEnabled: true
            Accessible.name: modelData.label
            onClicked: picker.choose(modelData)
            background: Item {}
            contentItem: Column {
                spacing: 6
                // A page being read: panels, with the bar on it.
                Rectangle {
                    width: tile.width
                    height: picker.tileHeight
                    radius: Theme.radiusMd
                    color: "#17171b"
                    clip: true
                    border.width: tile.selected ? 2 : 1
                    border.color: tile.selected ? Theme.accent : tile.hovered ? Theme.borderStrong : Theme.border
                    Column {
                        x: 14
                        y: 12
                        spacing: 6
                        Repeater {
                            model: [0.62, 0.38, 0.5]
                            Rectangle {
                                required property real modelData
                                width: tile.width - 28
                                height: (picker.tileHeight - 36) * modelData / 1.5
                                radius: 3
                                color: "#2c2c33"
                            }
                        }
                    }
                    ReaderProgress {
                        visible: tile.modelData.key !== "none"
                        anchors.fill: parent
                        // Inside the border, which is 2 px when chosen.
                        anchors.margins: 3
                        interactive: false
                        count: 12
                        current: 4
                        look: tile.modelData.key
                        // The chosen edge on the chosen look, else its own.
                        edge: (tile.selected ? (App.settings.readerProgressEdge || tile.modelData.edge) : tile.modelData.edge) || ""
                        thickness: Math.max(3, Number(App.settings.readerProgressWidth) || 3)
                        unread: App.settings.readerRailUnread || "#e4e4e7"
                        read: App.settings.readerRailRead || "#8b8b94"
                        currentColor: App.settings.readerRailCurrent || "#000000"
                        strength: App.settings.readerProgressOpacity !== undefined ? Number(App.settings.readerProgressOpacity) : 0.85
                    }
                    Icon {
                        visible: tile.modelData.key === "none"
                        anchors.centerIn: parent
                        name: "eye-off"
                        size: 22
                        color: "#8a8a92"
                    }
                }
                Text {
                    width: tile.width
                    horizontalAlignment: Text.AlignHCenter
                    text: tile.modelData.label
                    color: tile.selected ? Theme.text : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: tile.selected ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }
            }
        }
    }
}
