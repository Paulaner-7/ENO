import QtQuick
import Scancore

// The last searches on a search page, as chips that run them again. Kept in
// settings under `key`, newest first.
Column {
    id: root
    property string key
    signal picked(string text)

    // From C++ as a list object, not a JavaScript array.
    readonly property var items: Array.from(App.settings[key] || [])

    function remember(text) {
        const t = String(text).trim()
        if (!t || /^https?:\/\//.test(t)) return
        const lower = t.toLowerCase()
        App.setSetting(key, [t].concat(items.filter(x => String(x).toLowerCase() !== lower)).slice(0, 8))
    }

    readonly property bool hasContent: items.length > 0
    visible: hasContent
    width: parent ? parent.width : 400
    spacing: 10

    Item {
        width: parent.width
        height: 26
        Text {
            x: Theme.gutter
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Recent searches")
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.1
        }
        ScButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.gutter - 6
            anchors.verticalCenter: parent.verticalCenter
            variant: "ghost"
            compact: true
            text: qsTr("Clear")
            onClicked: App.setSetting(root.key, [])
        }
    }
    ChipRow {
        Repeater {
            model: root.items
            Chip {
                required property var modelData
                text: String(modelData)
                iconName: "history"
                onClicked: root.picked(String(modelData))
            }
        }
    }
}
