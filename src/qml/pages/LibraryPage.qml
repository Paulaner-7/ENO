import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Saved comics, novels and anime, with update checks.
Item {
    id: page
    property string media: ""
    property string filter: ""
    property var task: Library.entries("")
    property var checkTask: null

    readonly property string sort: App.settings.librarySort || "updates"
    // The last result, kept while a refresh runs.
    property var all: []
    Connections {
        target: page.task
        function onFinished(result) { page.all = Router.keep(page.all, result) }
    }
    readonly property var entries: {
        const f = filter.trim().toLowerCase()
        const out = page.all.filter(e => (!media || e.media === media) && (!f || (e.title || "").toLowerCase().includes(f)))
        // "updates" keeps the stored order: new chapters first, then recent.
        if (sort === "recent") out.sort((a, b) => (b.progressAt || 0) - (a.progressAt || 0))
        else if (sort === "added") out.sort((a, b) => (b.addedAt || 0) - (a.addedAt || 0))
        else if (sort === "title") out.sort((a, b) => String(a.title || "").localeCompare(String(b.title || "")))
        return out
    }

    function search(text) { filter = text }

    property bool dirty: false
    onVisibleChanged: if (visible && dirty) { dirty = false; refresh.restart() }

    Connections {
        target: Library
        function onChanged() {
            if (page.visible) refresh.restart()
            else page.dirty = true
        }
    }
    Timer { id: refresh; interval: 300; onTriggered: page.task = Library.entries("") }
    Connections {
        target: page.checkTask
        function onFinished(result) {
            Router.toast(result.newUnits > 0 ? result.newUnits + qsTr(" new chapters found") : qsTr("Everything is up to date"),
                         result.newUnits > 0 ? "success" : "info")
        }
        function onFailed(error) { Router.toast(error, "error") }
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 22

            PageHeader {
                title: qsTr("Library")
                subtitle: page.all.length + qsTr(" saved")
                    + (Library.unreadUpdates > 0 ? " · " + Library.unreadUpdates + qsTr(" new chapters") : "")
                ScButton {
                    text: page.checkTask && page.checkTask.loading ? qsTr("Checking…") : qsTr("Check for updates")
                    iconName: "refresh-cw"
                    loading: page.checkTask ? page.checkTask.loading : false
                    onClicked: page.checkTask = Library.checkUpdates()
                }
            }

            Flow {
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 12
                Segmented {
                    options: [{ value: "", label: qsTr("All") }, { value: "comic", label: qsTr("Comics"), icon: "book-open" },
                              { value: "novel", label: qsTr("Novels"), icon: "book-text" }, { value: "anime", label: qsTr("Anime"), icon: "tv" }]
                    value: page.media
                    onPicked: (v) => page.media = v
                }
                SearchField {
                    width: Theme.compact ? parent.width : 260
                    placeholderText: qsTr("Filter library")
                    text: page.filter
                    onTextChanged: page.filter = text
                }
                Select {
                    implicitWidth: 196
                    model: [{ value: "updates", label: qsTr("New chapters first") }, { value: "recent", label: qsTr("Recently read") },
                            { value: "added", label: qsTr("Recently added") }, { value: "title", label: qsTr("Title A–Z") }]
                    value: page.sort
                    onPicked: (v) => App.setSetting("librarySort", v)
                }
            }

            CoverGrid {
                items: page.entries
                badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                progressFor: (it) => it.unitKey ? (it.completed ? 1 : (it.percent || 0)) : -1
                subtitleFor: (it) => it.unitTitle
                    ? (it.media === "anime" ? qsTr("Episode ") + (it.unitNumber || it.unitKey) : it.unitTitle)
                    : (it.media === "anime" ? qsTr("Anime") : it.media === "novel" ? qsTr("Novel") : qsTr("Comic"))
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }

            StateView {
                width: parent.width
                task: page.all.length === 0 ? page.task : null
                empty: page.task && page.task.ok && page.entries.length === 0
                emptyTitle: page.filter || page.media ? qsTr("Nothing matches") : qsTr("Your library is empty")
                emptyText: qsTr("Open any comic, novel or anime and choose “Add to library” to keep it here. SCANCORE checks saved series for new chapters in the background.")
                emptyIcon: "library"
                onRetry: page.task = Library.entries("")
            }
        }
    }

    BackToTop { flick: flick }
}
