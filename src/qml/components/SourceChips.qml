import QtQuick
import ScanCore

// The source chips under a search field: "All sources", then one per source
// with its result count while a search runs. Installed extensions can add
// hundreds of sources, so it is a ListView (only the chips on screen exist)
// on one line that scrolls sideways; wide windows with few sources wrap
// them instead. `providers` maps a source key to the search's provider
// entry ({pending, ok, count}), looked up once per update, not per chip.
Item {
    id: root
    property var sources: []
    property string current: "all"
    property var providers: ({})
    property bool showLanguage: false
    // Wrapping is fine for a handful; past this the chips scroll.
    property int wrapLimit: 30
    signal picked(string key)

    readonly property var entries: [{ key: "all", label: qsTr("All sources") }].concat(sources)
    readonly property bool scrolls: Theme.compact || entries.length > wrapLimit

    width: parent ? parent.width : 400
    height: scrolls ? 32 : wrapped.height

    function label(s) {
        return s.label + (showLanguage && s.language && s.language !== "en" ? " · " + s.language.toUpperCase() : "")
    }
    function detail(key) {
        const p = providers[key]
        return !p ? "" : p.pending ? "…" : p.ok ? String(p.count) : "!"
    }

    component SourceChip: Chip {
        required property var modelData
        text: root.label(modelData)
        selected: root.current === modelData.key
        detail: modelData.key === "all" ? "" : root.detail(modelData.key)
        onClicked: root.picked(modelData.key)
    }

    ListView {
        visible: root.scrolls
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: 8
        leftMargin: Theme.gutter
        rightMargin: Theme.gutter
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Built ahead a little so a fling does not show gaps.
        cacheBuffer: 400
        model: root.scrolls ? root.entries : []
        delegate: SourceChip {}
    }

    Flow {
        id: wrapped
        visible: !root.scrolls
        x: Theme.gutter
        width: parent.width - 2 * Theme.gutter
        spacing: 8
        Repeater {
            model: root.scrolls ? [] : root.entries
            SourceChip {}
        }
    }
}
