import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Saved comics, novels and anime, each on its own: categories (reading,
// planned, on hold, completed, dropped), favourites, three views (grid,
// compact grid, list) and update checks.
Item {
    id: page
    property string filter: ""
    property var task: Library.entries("", true)
    property var checkTask: null
    // Select: pick several series to move, list or remove together.
    property bool selecting: false
    property var selection: ({})
    readonly property int selectedCount: Object.keys(selection).length
    onMediaChanged: { selecting = false; selection = ({}) }

    // ENO: anime only, whatever was picked before.
    readonly property string media: Router.animeOnly ? "anime" : (App.settings.libraryMedia || "comic")
    readonly property string category: App.settings.libraryCategory || "all"
    readonly property string view: App.settings.libraryView || "grid"
    readonly property string sort: (Router.animeOnly && (!App.settings.librarySort || App.settings.librarySort === "updates")) ? "recent" : (App.settings.librarySort || "updates")
    readonly property var categories: Router.libraryCategories(media)

    // The last result, kept while a refresh runs.
    property var all: []
    Connections {
        target: page.task
        function onFinished(result) { page.all = Router.keep(page.all, result) }
    }
    readonly property var inMedia: all.filter(e => e.media === media)
    // How many series each chip holds.
    readonly property var counts: {
        const c = { all: inMedia.length, favourites: 0 }
        for (const e of inMedia) {
            const k = e.category || "reading"
            c[k] = (c[k] || 0) + 1
            if (e.favourite) c.favourites += 1
        }
        return c
    }
    readonly property var entries: {
        const f = filter.trim().toLowerCase()
        const out = inMedia.filter(e => (category === "all" || (category === "favourites" ? !!e.favourite : (e.category || "reading") === category))
                                        && (!f || (e.title || "").toLowerCase().includes(f)))
        // "updates" keeps the stored order: new chapters first, then recent.
        if (sort === "recent") out.sort((a, b) => (b.progressAt || 0) - (a.progressAt || 0))
        else if (sort === "added") out.sort((a, b) => (b.addedAt || 0) - (a.addedAt || 0))
        else if (sort === "title") out.sort((a, b) => String(a.title || "").localeCompare(String(b.title || "")))
        return out
    }
    readonly property string categoryLabel: category === "favourites" ? qsTr("Favourites")
        : (categories.find(c => c.value === category) || { label: "" }).label

    readonly property bool checking: checkTask ? checkTask.loading : false
    readonly property var mediaOptions: Router.reachable([{ value: "comic", media: "comic", label: qsTr("Comics"), icon: "book-open" },
        { value: "novel", media: "novel", label: qsTr("Novels"), icon: "book-text" }, { value: "anime", media: "anime", label: qsTr("Anime"), icon: "tv" }])
    readonly property var sortOptions: (Router.animeOnly ? [] : [{ value: "updates", label: qsTr("New chapters first") }]).concat([
        { value: "recent", label: media === "anime" ? qsTr("Recently watched") : qsTr("Recently read") },
        { value: "added", label: qsTr("Recently added") }, { value: "title", label: qsTr("Title A–Z") }])
    readonly property var viewOptions: [{ value: "grid", icon: "layout-grid", label: qsTr("Grid") },
        { value: "compact", icon: "grid-2x2", label: qsTr("Compact grid") }, { value: "list", icon: "list", label: qsTr("List") }]

    function search(text) { filter = text }
    function toggleSelecting() { if (selecting) endSelection(); else selecting = true }
    function toggle(it) {
        const s = Object.assign({}, selection)
        if (s[it.id]) delete s[it.id]
        else s[it.id] = true
        selection = s
    }
    function selectAll() {
        const s = {}
        for (const e of entries) s[e.id] = true
        selection = s
    }
    function endSelection() { selecting = false; selection = ({}) }
    function moveSelected() {
        const list = []
        for (const id of Object.keys(selection)) {
            const row = Library.seriesRow(id)
            if (!row.id || String(row.sourceUrl || "").startsWith("file:")) continue
            list.push({ id: row.id, media: row.media, title: row.title, cover: row.cover || "", source: row.source,
                        sourceLabel: row.media === "novel" ? ((Novels.sources().find(x => x.key === row.source) || {}).label || row.source)
                                                         : Comics.sourceLabel(row.source),
                        sourceUrl: row.sourceUrl, extra: row.extra || {} })
        }
        if (list.length === 0) { Router.toast(qsTr("Files on this device stay where they are."), "info"); return }
        migrate.start(list)
    }
    function subtitle(it) {
        return it.unitTitle
            ? (it.media === "anime" ? qsTr("Episode ") + (it.unitNumber || it.unitKey) : it.unitTitle)
            : (it.media === "anime" ? qsTr("Anime") : it.media === "novel" ? qsTr("Novel") : qsTr("Comic"))
    }

    property bool dirty: false
    onVisibleChanged: if (visible && dirty) { dirty = false; refresh.restart() }

    Connections {
        target: Library
        function onChanged() {
            if (page.visible) refresh.restart()
            else page.dirty = true
        }
    }
    Timer { id: refresh; interval: 300; onTriggered: page.task = Library.entries("", true) }
    Connections {
        target: page.checkTask
        function onFinished(result) {
            Router.toast(result.newUnits > 0 ? result.newUnits + qsTr(" new chapters found") : qsTr("Everything is up to date"),
                         result.newUnits > 0 ? "success" : "info")
        }
        function onFailed(error) { Router.toast(error, "error") }
    }

    ActionMenu { id: menu }
    MigrateDialog {
        id: migrate
        onClosed: page.endSelection()
    }
    ConfirmDialog {
        id: removeSelected
        heading: page.selectedCount === 1 ? qsTr("Remove 1 series from your library?") : qsTr("Remove %1 series from your library?").arg(page.selectedCount)
        message: qsTr("Your reading history and downloads are kept.")
        confirmText: qsTr("Remove")
        destructive: true
        onConfirmed: {
            for (const id of Object.keys(page.selection)) Library.remove(id)
            page.endSelection()
        }
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: Theme.compact ? 14 : 20

            PageHeader {
                keepBeside: Theme.compact
                title: qsTr("Library")
                subtitle: (Router.animeOnly ? page.inMedia.length : page.all.length) + qsTr(" saved")
                    + (Library.unreadUpdates > 0 ? " · " + Library.unreadUpdates + (Router.animeOnly ? qsTr(" new episodes") : qsTr(" new chapters")) : "")
                ScButton {
                    visible: !Theme.compact
                    text: page.selecting ? qsTr("Done") : qsTr("Select")
                    iconName: page.selecting ? "x" : "list-checks"
                    variant: "ghost"
                    onClicked: page.toggleSelecting()
                }
                ScButton {
                    visible: !Theme.compact
                    text: qsTr("Upcoming", "release radar")
                    iconName: "calendar"
                    variant: "ghost"
                    onClicked: Router.openUpcoming()
                }
                // New chapter checks are for comics and novels only.
                ScButton {
                    visible: !Theme.compact && !Router.animeOnly
                    text: page.checking ? qsTr("Checking…") : qsTr("Check for updates")
                    iconName: "refresh-cw"
                    loading: page.checking
                    onClicked: page.checkTask = Library.checkUpdates()
                }
                // Phones: the same two as icons, so the title keeps its row.
                IconButton {
                    visible: Theme.compact
                    diameter: 40
                    filled: !page.selecting
                    iconName: page.selecting ? "x" : "list-checks"
                    active: page.selecting
                    tip: page.selecting ? qsTr("Done") : qsTr("Select")
                    Accessible.name: tip
                    onClicked: page.toggleSelecting()
                }
                IconButton {
                    visible: Theme.compact
                    diameter: 40
                    filled: true
                    iconName: "calendar"
                    tip: qsTr("Upcoming", "release radar")
                    Accessible.name: tip
                    onClicked: Router.openUpcoming()
                }
                IconButton {
                    id: checkButton
                    visible: Theme.compact && !Router.animeOnly
                    diameter: 40
                    filled: true
                    iconName: "refresh-cw"
                    enabled: !page.checking
                    tip: page.checking ? qsTr("Checking…") : qsTr("Check for updates")
                    Accessible.name: tip
                    onClicked: page.checkTask = Library.checkUpdates()
                    RotationAnimator on rotation {
                        running: page.checking && !Motion.reduced
                        loops: Animation.Infinite
                        from: 0; to: 360; duration: 900
                        onRunningChanged: if (!running) checkButton.rotation = 0
                    }
                }
            }

            // Sources that stopped answering, or series saved twice.
            Rectangle {
                id: careBanner
                visible: !Router.animeOnly && Upkeep.attention > 0
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                height: visible ? careRow.height + 20 : 0
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                Item {
                    id: careRow
                    x: 14
                    y: 10
                    width: parent.width - 28
                    height: Math.max(careText.implicitHeight, reviewButton.height)
                    Icon { id: careIcon; anchors.verticalCenter: parent.verticalCenter; name: "activity"; size: 18; color: Theme.warning }
                    Text {
                        id: careText
                        anchors.left: careIcon.right
                        anchors.leftMargin: 12
                        anchors.right: reviewButton.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: Upkeep.attention === 1 ? qsTr("Something in your library needs a look: a source that stopped answering or a series saved twice.")
                            : qsTr("%1 things in your library need a look: sources that stopped answering or series saved twice.").arg(Upkeep.attention)
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                    ScButton {
                        id: reviewButton
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        compact: true
                        text: qsTr("Review")
                        onClicked: Router.openLibraryCare()
                    }
                }
            }

            Segmented {
                visible: Theme.compact && page.mediaOptions.length > 1
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                fill: true
                options: page.mediaOptions
                value: page.media
                onPicked: (v) => App.setSetting("libraryMedia", v)
            }

            // Phones: the filter with sort and view as icon buttons beside it.
            Row {
                visible: Theme.compact
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 8
                SearchField {
                    width: parent.width - 2 * (40 + parent.spacing)
                    placeholderText: qsTr("Filter library")
                    text: page.filter
                    onTextChanged: page.filter = text
                }
                IconButton {
                    id: sortButton
                    anchors.verticalCenter: parent.verticalCenter
                    diameter: 40
                    filled: true
                    iconName: "arrow-up-down"
                    tip: qsTr("Sort")
                    Accessible.name: tip
                    onClicked: menu.openAt(sortButton, 0, height + 6, page.sortOptions.map(o => ({
                        text: o.label, checked: o.value === page.sort, run: () => App.setSetting("librarySort", o.value) })))
                }
                IconButton {
                    id: viewButton
                    anchors.verticalCenter: parent.verticalCenter
                    diameter: 40
                    filled: true
                    iconName: (page.viewOptions.find(o => o.value === page.view) || page.viewOptions[0]).icon
                    tip: qsTr("View")
                    Accessible.name: tip
                    onClicked: menu.openAt(viewButton, 0, height + 6, page.viewOptions.map(o => ({
                        text: o.label, icon: o.icon, checked: o.value === page.view, run: () => App.setSetting("libraryView", o.value) })))
                }
            }

            Flow {
                visible: !Theme.compact
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 12
                Segmented {
                    visible: page.mediaOptions.length > 1
                    options: page.mediaOptions
                    value: page.media
                    onPicked: (v) => App.setSetting("libraryMedia", v)
                }
                SearchField {
                    width: 240
                    placeholderText: qsTr("Filter library")
                    text: page.filter
                    onTextChanged: page.filter = text
                }
                Select {
                    implicitWidth: 196
                    model: page.sortOptions
                    value: page.sort
                    onPicked: (v) => App.setSetting("librarySort", v)
                }
                Segmented {
                    options: page.viewOptions.map(o => ({ value: o.value, icon: o.icon }))
                    value: page.view
                    onPicked: (v) => App.setSetting("libraryView", v)
                }
            }

            ChipRow {
                Chip {
                    text: qsTr("All")
                    detail: String(page.counts.all || 0)
                    selected: page.category === "all"
                    onClicked: App.setSetting("libraryCategory", "all")
                }
                Chip {
                    text: qsTr("Favourites")
                    iconName: "heart"
                    detail: String(page.counts.favourites || 0)
                    selected: page.category === "favourites"
                    onClicked: App.setSetting("libraryCategory", "favourites")
                }
                Repeater {
                    model: page.categories
                    Chip {
                        required property var modelData
                        text: modelData.label
                        detail: String(page.counts[modelData.value] || 0)
                        selected: page.category === modelData.value
                        onClicked: App.setSetting("libraryCategory", modelData.value)
                    }
                }
            }

            // What to do with the series picked.
            Flow {
                visible: page.selecting
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 8
                Text {
                    height: 36
                    verticalAlignment: Text.AlignVCenter
                    text: page.selectedCount === 0 ? qsTr("Tap series to pick them") : qsTr("%1 picked").arg(page.selectedCount)
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                    rightPadding: 8
                }
                ScButton {
                    compact: true
                    text: qsTr("All")
                    onClicked: page.selectAll()
                }
                ScButton {
                    visible: page.media !== "anime"
                    compact: true
                    iconName: "arrow-right-left"
                    text: qsTr("Move to another source")
                    enabled: page.selectedCount > 0
                    onClicked: page.moveSelected()
                }
                Select {
                    implicitWidth: 170
                    enabled: page.selectedCount > 0
                    model: [{ value: "", label: qsTr("Move to list…") }].concat(page.categories)
                    value: ""
                    onPicked: (v) => {
                        currentIndex = 0
                        if (!v) return
                        for (const id of Object.keys(page.selection)) Library.setCategory(id, v)
                        Router.toast(qsTr("Moved to %1").arg(page.categories.find(c => c.value === v).label), "success")
                        page.endSelection()
                    }
                }
                ScButton {
                    compact: true
                    iconName: "trash"
                    text: qsTr("Remove")
                    enabled: page.selectedCount > 0
                    onClicked: removeSelected.open()
                }
            }

            CoverGrid {
                visible: page.view !== "list"
                items: page.view !== "list" ? page.entries : []
                minCardWidth: page.view === "compact" ? Math.round((Theme.compact ? 78 : 112) * Theme.coverScale)
                                                      : Math.round((Theme.compact ? 104 : 150) * Theme.coverScale)
                badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                progressFor: (it) => it.unitKey ? (it.completed ? 1 : (it.percent || 0)) : -1
                subtitleFor: (it) => page.subtitle(it)
                markedFor: { const s = page.selection; return (it) => !!s[it.id] }
                onOpened: (it, cover) => page.selecting ? page.toggle(it) : Router.openSeries(it, cover)
            }
            CoverList {
                visible: page.view === "list"
                items: page.view === "list" ? page.entries : []
                badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                progressFor: (it) => it.unitKey ? (it.completed ? 1 : (it.percent || 0)) : -1
                subtitleFor: (it) => [page.subtitle(it), it.sourceLabel || ""].filter(s => s).join(" · ")
                markedFor: { const s = page.selection; return (it) => !!s[it.id] }
                onOpened: (it, cover) => page.selecting ? page.toggle(it) : Router.openSeries(it, cover)
            }

            StateView {
                width: parent.width
                task: page.all.length === 0 ? page.task : null
                empty: page.task && page.task.ok && page.entries.length === 0
                emptyTitle: page.filter ? qsTr("Nothing matches")
                    : page.all.length === 0 ? qsTr("Your library is empty")
                    : page.category === "all" ? qsTr("Nothing saved here yet")
                    : qsTr("Nothing in %1").arg(page.categoryLabel)
                emptyText: page.all.length === 0 || page.category === "all"
                    ? qsTr("Open any comic, novel or anime and choose “Add to library” to keep it here. SCANCORE checks saved series for new chapters in the background.")
                    : page.category === "favourites" ? qsTr("Tap the heart on a saved series to keep it here.")
                    : qsTr("Choose a category on a saved series' page to move it here.")
                emptyIcon: page.category === "favourites" ? "heart" : "library"
                onRetry: page.task = Library.entries("", true)
            }
        }
    }

    BackToTop { flick: flick }
}
