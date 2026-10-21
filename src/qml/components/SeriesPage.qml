import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Details page shared by comics and novels: header, actions and a fast
// chapter list with read state, downloads and multi-select.
Item {
    id: page
    property string media: "comic"
    property string url
    property var summary: ({})
    property bool autoResume: false

    property var task: load()
    property string filter: ""
    property bool selecting: false
    // Chapters, Reviews or Discussion.
    property string tab: "chapters"
    // Chapters are listed ten at a time.
    readonly property int perPage: 10
    property int pageIndex: 0
    // A comic or book file on this device: nothing to download or open online.
    readonly property bool local: String(url).startsWith("file:")
    property var selection: ({})

    function load() {
        return media === "novel" ? Novels.book(url, summary) : Comics.series(url, summary)
    }

    readonly property var series: {
        const base = Object.assign({ media: media, sourceUrl: url }, summary)
        if (task && task.ok) return Object.assign(base, task.result.series)
        return base
    }
    readonly property string seriesId: series.id || (media === "novel" ? Novels.seriesId(url) : Comics.seriesId(url))
    // Until the site answers, the chapter list saved last time (if any).
    readonly property var storedChapters: Library.storedUnits(seriesId)
    readonly property var chapters: task && task.ok ? task.result.chapters : storedChapters
    readonly property var progress: { App.dependOn(Library.revision); return Library.progressMap(seriesId) }
    readonly property var downloads: { App.dependOn(Downloads.revision); return Downloads.states(seriesId) }
    readonly property bool saved: { App.dependOn(Library.revision); return Library.isSaved(seriesId) }
    readonly property var bookmarks: { App.dependOn(Library.revision); return Library.bookmarks(seriesId) }
    // This series' own chapter list settings: sort and filters.
    readonly property var chapterSettings: { App.dependOn(Library.revision); return Library.seriesSettings(seriesId) }
    readonly property bool descending: chapterSettings.sort !== "asc"
    readonly property bool onlyUnread: chapterSettings.onlyUnread === true
    readonly property bool onlyDownloaded: chapterSettings.onlyDownloaded === true
    readonly property bool onlyBookmarked: chapterSettings.onlyBookmarked === true
    readonly property string scanlator: chapterSettings.scanlator || ""
    readonly property int filtersOn: (onlyUnread ? 1 : 0) + (onlyDownloaded ? 1 : 0) + (onlyBookmarked ? 1 : 0) + (scanlator ? 1 : 0)
    // The groups that translated chapters here, most chapters first.
    readonly property var scanlators: {
        const counts = {}
        for (const c of chapters) if (c.scanlator) counts[c.scanlator] = (counts[c.scanlator] || 0) + 1
        return Object.keys(counts).sort((a, b) => counts[b] - counts[a])
    }
    function setChapterSetting(key, value) { Library.setSeriesSetting(seriesId, key, value) }
    readonly property string coverReferer: media === "comic" ? Comics.imageReferer(url) : ""
    // Comick's catalogue id when the comic was opened from the catalogue; it
    // ties the series to its comments, reviews and tracking.
    readonly property string catalogId: (series.extra && series.extra.hid) || summary.hid || ""

    property var anilistTask: Trackers.anilistDetails(seriesId, media, series.title || "")
    readonly property var anilist: anilistTask && anilistTask.ok ? anilistTask.result : ({})

    Connections {
        target: Trackers
        function onChanged() {
            page.anilistTask = Trackers.anilistDetails(page.seriesId, page.media, page.series.title || "")
        }
    }

    readonly property var resumeInfo: {
        App.dependOn(Library.revision)
        if (chapters.length === 0) return { index: 0, label: page.media === "novel" ? qsTr("Start reading") : qsTr("Start reading"), detail: "" }
        const last = Library.lastProgress(seriesId)
        if (last && last.unitKey) {
            const i = chapters.findIndex(c => c.key === last.unitKey)
            if (i >= 0) {
                if (last.completed && i + 1 < chapters.length)
                    return { index: i + 1, label: qsTr("Continue"), detail: qsTr("Next: ") + chapters[i + 1].title }
                if (last.completed)
                    return { index: i, label: qsTr("Read again"), detail: qsTr("You finished ") + chapters[i].title }
                return { index: i, label: qsTr("Continue"), detail: chapters[i].title + (last.percent > 0.02 ? " · " + Math.round(last.percent * 100) + "%" : "") }
            }
        }
        return { index: 0, label: qsTr("Start reading"), detail: chapters[0].title }
    }

    readonly property var rows: {
        const out = []
        const f = filter.trim().toLowerCase()
        for (let i = 0; i < chapters.length; ++i) {
            const c = chapters[i]
            if (f && !(c.title || "").toLowerCase().includes(f) && String(c.number) !== f) continue
            if (onlyUnread && progress[c.key] && progress[c.key].completed) continue
            if (onlyDownloaded && !(downloads[c.key] && downloads[c.key].status === "done")) continue
            if (onlyBookmarked && !bookmarks[c.key]) continue
            if (scanlator && c.scanlator !== scanlator) continue
            out.push({ unit: c, index: i })
        }
        return descending ? out.reverse() : out
    }
    readonly property int pageCount: Math.max(1, Math.ceil(rows.length / perPage))
    readonly property var pageRows: rows.slice(pageIndex * perPage, (pageIndex + 1) * perPage)
    // Opens on the page of the chapter you are on.
    function showCurrentPage() {
        if (!rows || !resumeInfo) return
        // Nothing read yet: the first page.
        const last = Library.lastProgress(seriesId)
        const at = last && last.unitKey ? rows.findIndex(r => r.index === resumeInfo.index) : -1
        pageIndex = filter.trim() || at < 0 ? 0 : Math.floor(at / perPage)
    }
    onFilterChanged: showCurrentPage()
    onFiltersOnChanged: showCurrentPage()
    onScanlatorChanged: showCurrentPage()
    onPageCountChanged: if (pageIndex >= pageCount) pageIndex = pageCount - 1
    onDescendingChanged: showCurrentPage()
    onChaptersChanged: showCurrentPage()
    onPageIndexChanged: if (list.contentY - list.originY > (list.headerItem ? list.headerItem.height : 0)) list.positionViewAtBeginning()

    function openAt(index) {
        if (index < 0 || index >= chapters.length) return
        Library.markSeen(seriesId)
        if (media === "novel") Router.openNovelReader(series, chapters, index)
        else Router.openComicReader(series, chapters, index)
    }

    function toggleSave() {
        if (saved) {
            Library.remove(seriesId)
            Router.toast(qsTr("Removed from your library"))
        } else {
            Library.save(series)
            Router.toast(qsTr("Added to your library"), "success")
        }
    }

    function selectedUnits() { return chapters.filter(c => selection[c.key]) }
    function endSelection() { selecting = false; selection = ({}) }
    // Everything before a chapter, in reading order.
    function before(index) { return chapters.slice(0, index) }
    function showMenu(row, x, y, unit, index) {
        const read = progress[unit.key] && progress[unit.key].completed
        const marked = !!bookmarks[unit.key]
        const dl = downloads[unit.key]
        const items = [
            { text: marked ? qsTr("Remove bookmark") : qsTr("Bookmark"), icon: marked ? "bookmark-check" : "bookmark-plus",
              run: () => Library.setBookmarked(seriesId, [unit.key], !marked) },
            { text: read ? qsTr("Mark unread") : qsTr("Mark read"), icon: read ? "eye-off" : "eye",
              run: () => Library.markUnits(series, [unit], !read) },
            { text: qsTr("Mark previous as read"), icon: "check-check", enabled: index > 0,
              run: () => Library.markUnits(series, before(index), true) },
            { text: qsTr("Mark previous as unread"), icon: "undo-2", enabled: index > 0,
              run: () => Library.markUnits(series, before(index), false) }
        ]
        if (!local) {
            items.push(null)
            items.push(dl && dl.status === "done"
                ? { text: qsTr("Remove download"), icon: "trash", danger: true, run: () => Downloads.removeUnit(seriesId, unit.key) }
                : { text: qsTr("Download"), icon: "download", run: () => download([unit]) })
            items.push({ text: qsTr("Download from here on"), icon: "download",
                         run: () => download(chapters.slice(index).filter(c => !(downloads[c.key] && downloads[c.key].status === "done"))) })
        }
        chapterMenu.openAt(row, x, y, items)
    }

    function download(units) {
        if (units.length === 0) return
        Downloads.enqueue(series, units)
        Router.toast(qsTr("Downloading ") + units.length + (units.length === 1 ? qsTr(" chapter") : qsTr(" chapters")))
    }

    // Continue straight from the saved chapter list; the fresh one replaces
    // it on this page when it arrives.
    Component.onCompleted: {
        if (autoResume && storedChapters.length > 0) {
            autoResume = false
            Qt.callLater(() => page.openAt(page.resumeInfo.index))
        }
    }

    Connections {
        target: page.task
        function onFinished() {
            // The header grows once details arrive; keep the page at its top
            // unless the reader has already scrolled.
            if (!list.moving && list.contentY - list.originY < 40)
                Qt.callLater(() => list.positionViewAtBeginning())
            if (page.autoResume) {
                page.autoResume = false
                page.openAt(page.resumeInfo.index)
            }
        }
    }

    ActionMenu { id: chapterMenu }

    // The chapter list's filters, saved with the series.
    Popup {
        id: chapterFilters
        parent: Overlay.overlay
        x: Math.max(8, Math.min(filterAnchor.x, (parent ? parent.width : 400) - width - 8))
        y: filterAnchor.y
        property point filterAnchor: Qt.point(0, 0)
        width: 300
        padding: 14
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: Theme.borderStrong
        }
        contentItem: Column {
            spacing: 4
            Repeater {
                model: [{ key: "onlyUnread", label: qsTr("Unread only") },
                        { key: "onlyDownloaded", label: qsTr("Downloaded only"), hidden: page.local },
                        { key: "onlyBookmarked", label: qsTr("Bookmarked only") }]
                Item {
                    required property var modelData
                    visible: !modelData.hidden
                    width: parent.width
                    height: visible ? 40 : 0
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.modelData.label
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                    }
                    Toggle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.chapterSettings[parent.modelData.key] === true
                        onToggled: page.setChapterSetting(parent.modelData.key, checked ? true : null)
                    }
                }
            }
            Text {
                visible: page.scanlators.length > 1
                topPadding: 8
                text: qsTr("Translated by")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textXs
            }
            Select {
                visible: page.scanlators.length > 1
                width: parent.width
                model: [{ value: "", label: qsTr("Every group") }].concat(page.scanlators.map(s => ({ value: s, label: s })))
                value: page.scanlator
                onPicked: (v) => page.setChapterSetting("scanlator", v || null)
            }
            ScButton {
                visible: page.filtersOn > 0
                topPadding: 8
                compact: true
                variant: "ghost"
                iconName: "funnel-x"
                text: qsTr("Clear filters")
                onClicked: {
                    for (const k of ["onlyUnread", "onlyDownloaded", "onlyBookmarked", "scanlator"]) page.setChapterSetting(k, null)
                }
            }
        }
    }

    TrackingDialog {
        id: tracking
        seriesId: page.seriesId
        media: page.media
        seriesTitle: page.series.title || ""
    }

    ReadFromDialog { id: readFrom }

    MigrateDialog {
        id: migrate
        onMigrated: (result) => {
            // The series lives on the new source now: its page replaces this one.
            Router.back()
            Router.openSeries(result.series, null)
        }
    }

    ListView {
        WheelScroll {}
        id: list
        anchors.fill: parent
        model: page.tab === "chapters" ? page.pageRows : []
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 5000
        cacheBuffer: 1200
        reuseItems: true
        ScrollBar.vertical: ScScrollBar {}
        bottomMargin: 40

        Component.onCompleted: Qt.callLater(() => { if (headerItem) Router.shell.land(headerItem.details.coverItem) })

        header: Column {
            readonly property alias details: details
            width: list.width
            spacing: 0

            DetailsHeader {
                id: details
                series: page.series
                anilist: page.anilist
                coverReferer: page.coverReferer
                kicker: (page.series.sourceLabel || page.series.source || "") + (page.media === "novel" ? qsTr(" · Novel") : qsTr(" · Comic"))
                meta: [
                    page.series.status || (page.anilist && page.anilist.status ? page.anilist.status : ""),
                    page.chapters.length ? page.chapters.length + (page.media === "novel" ? qsTr(" chapters") : qsTr(" chapters")) : "",
                    page.series.type || (page.anilist && page.anilist.format ? page.anilist.format : ""),
                    (page.anilist && page.anilist.score) ? ("★ " + page.anilist.score) : (page.series.rating ? ("★ " + page.series.rating) : ""),
                    page.task && page.task.ok && page.task.result.offline ? qsTr("Offline copy") : ""
                ]
                primaryText: page.resumeInfo.label
                primaryDetail: page.resumeInfo.detail
                busy: page.task ? page.task.loading && page.chapters.length === 0 : false
                saved: page.saved
                onPrimary: page.openAt(page.resumeInfo.index)
                onToggleSaved: page.toggleSave()

                IconButton {
                    iconName: page.local ? "folder-open" : "external-link"
                    filled: true
                    tip: page.local ? qsTr("Show in its folder") : qsTr("Open on the website")
                    onClicked: {
                        if (page.local) App.openFolder(App.localPath(String(page.url).replace(/\/[^\/]*$/, "")))
                        else if (/^https?:/.test(page.url)) App.openUrl(page.url)
                        else {
                            // Extensions' and plugins' links are their own paths.
                            const task = page.media === "novel" ? Novels.webUrl(page.url) : Comics.webUrl(page.url)
                            task.finished.connect((link) => App.openUrl(link))
                            task.failed.connect((error) => Router.toast(error, "error"))
                        }
                    }
                }
                IconButton {
                    visible: !!page.anilist && !!page.anilist.url
                    iconName: "external-link"
                    filled: true
                    tip: qsTr("Open on AniList")
                    onClicked: App.openUrl(page.anilist.url)
                }
                IconButton {
                    visible: !page.local && Trackers.anyConnected
                    iconName: "link"
                    filled: true
                    tip: qsTr("Tracking")
                    onClicked: tracking.open()
                }
                // Found on a built-in source: read it from an installed
                // extension instead.
                IconButton {
                    readonly property bool builtIn: !!page.series.source
                        && !(page.media === "novel" ? Novels.isExtension(page.series.source) : Comics.isExtension(page.series.source))
                    visible: !page.local && builtIn && (page.media === "novel" ? Novels.hasExtensions() : Comics.hasExtensions())
                    iconName: "puzzle"
                    filled: true
                    tip: qsTr("Find it on my extensions")
                    onClicked: readFrom.start(migrate.itemFor({ media: page.media, title: page.series.title || "",
                                                                cover: page.series.cover || "", extra: page.series.extra || {} }), true)
                }
                IconButton {
                    visible: !page.local
                    iconName: "arrow-right-left"
                    filled: true
                    tip: qsTr("Move to another source")
                    onClicked: migrate.start([{ id: page.seriesId, media: page.media, title: page.series.title || "",
                                                cover: page.series.cover || "", source: page.series.source || "",
                                                sourceLabel: page.series.sourceLabel || "", sourceUrl: page.url,
                                                extra: page.series.extra || {} }])
                }
                IconButton {
                    iconName: "refresh-cw"
                    filled: true
                    tip: qsTr("Refresh")
                    onClicked: page.task = page.load()
                }
            }

            // A connected tracker that could not find this series by its
            // titles: one tap to pick the entry.
            Rectangle {
                id: missNotice
                readonly property var misses: { App.dependOn(Trackers.services, Library.revision); return page.local ? [] : Trackers.misses(page.seriesId) }
                readonly property var names: misses.map(k => k === "comick" ? qsTr("Comick") : (Trackers.services.find(s => s.key === k) || { label: k }).label)
                visible: misses.length > 0
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                height: visible ? missRow.implicitHeight + 24 : 0
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                Row {
                    id: missRow
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 28
                    spacing: 12
                    Icon { anchors.verticalCenter: parent.verticalCenter; name: "link-2"; color: Theme.warning; size: 18 }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 30 - pickButton.width - 24
                        text: qsTr("%1 could not find this by its title, so progress is not sent there.").arg(missNotice.names.join(", "))
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                    ScButton {
                        id: pickButton
                        anchors.verticalCenter: parent.verticalCenter
                        compact: true
                        text: qsTr("Pick the entry")
                        onClicked: tracking.pick(missNotice.misses[0])
                    }
                }
            }

            // The same story as an anime or a novel, and where each season
            // leaves off here.
            StoryPanel {
                visible: !page.local && forms.length > 0
                width: list.width
                bottomPadding: visible ? 8 : 0
                seriesId: page.local ? "" : page.seriesId
                media: page.media
                title: page.series.title || ""
                onOpenChapter: (number) => {
                    const index = page.chapters.findIndex(c => Number(c.number) === number)
                    if (index >= 0) page.openAt(index)
                    else Router.toast(qsTr("Chapter %1 is not on this source yet").arg(number))
                }
            }

            Item {
                width: list.width
                height: 60
                visible: !page.local
                Segmented {
                    x: Theme.gutter
                    anchors.verticalCenter: parent.verticalCenter
                    options: [{ value: "chapters", label: qsTr("Chapters") },
                              { value: "reviews", label: qsTr("Reviews") },
                              { value: "discussion", label: qsTr("Discussion") }]
                    value: page.tab
                    onPicked: (v) => page.tab = v
                }
            }

            Loader {
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                active: page.tab === "reviews"
                visible: active
                sourceComponent: Reviews {
                    media: page.media
                    title: page.series.title || ""
                    comickHid: page.catalogId
                    url: page.url
                }
            }
            Loader {
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                active: page.tab === "discussion"
                visible: active
                sourceComponent: Discussion {
                    media: page.media
                    title: page.series.title || ""
                    comickHid: page.catalogId
                    url: page.url
                }
            }

            // Toolbar
            Item {
                width: list.width
                height: 56
                visible: page.chapters.length > 0 || (page.task && page.task.loading)
                Rectangle {
                    x: Theme.gutter
                    width: parent.width - 2 * Theme.gutter
                    height: 1
                    color: Theme.border
                }
                Row {
                    x: Theme.gutter
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.chapters.length + (page.media === "novel" ? qsTr(" chapters") : qsTr(" chapters"))
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textLg
                        font.weight: Font.DemiBold
                    }
                    OutsideList {
                        view: list
                        outside: page
                        visible: !Theme.compact
                        width: 200
                        height: 34
                        anchors.verticalCenter: parent.verticalCenter
                        SearchField {
                            width: parent.width
                            implicitHeight: 34
                            placeholderText: qsTr("Find chapter")
                            onTextChanged: page.filter = text
                        }
                    }
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.gutter - 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    IconButton {
                        id: filterButton
                        iconName: "funnel"
                        active: page.filtersOn > 0
                        tip: page.filtersOn > 0 ? qsTr("Filters (%1 on)").arg(page.filtersOn) : qsTr("Filter chapters")
                        onClicked: {
                            const p = filterButton.mapToItem(Overlay.overlay, filterButton.width - chapterFilters.width, filterButton.height + 6)
                            chapterFilters.filterAnchor = p
                            chapterFilters.open()
                        }
                    }
                    IconButton {
                        iconName: page.selecting ? "x" : "list-checks"
                        active: page.selecting
                        tip: page.selecting ? qsTr("Cancel") : qsTr("Select chapters")
                        onClicked: {
                            page.selecting = !page.selecting
                            page.selection = ({})
                        }
                    }
                    IconButton {
                        iconName: "check"
                        tip: qsTr("Mark all as read")
                        onClicked: Library.markUnits(page.series, page.chapters, true)
                    }
                    IconButton {
                        iconName: "arrow-down-wide-narrow"
                        rotation: page.descending ? 0 : 180
                        tip: page.descending ? qsTr("Newest first") : qsTr("Oldest first")
                        Behavior on rotation { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }
                        onClicked: page.setChapterSetting("sort", page.descending ? "asc" : null)
                    }
                }
            }

            // What to do with the chapters picked.
            Flow {
                visible: page.selecting && page.tab === "chapters"
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                height: visible ? implicitHeight + 10 : 0
                spacing: 6
                readonly property int count: Object.keys(page.selection).length
                Text {
                    height: 32
                    verticalAlignment: Text.AlignVCenter
                    rightPadding: 6
                    text: parent.count === 0 ? qsTr("Tap chapters to pick them") : qsTr("%1 picked").arg(parent.count)
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }
                ScButton {
                    compact: true
                    text: qsTr("All")
                    onClicked: {
                        const s = {}
                        for (const r of page.rows) s[r.unit.key] = true
                        page.selection = s
                    }
                }
                ScButton {
                    compact: true
                    text: qsTr("Unread")
                    onClicked: {
                        const s = {}
                        for (const r of page.rows)
                            if (!(page.progress[r.unit.key] && page.progress[r.unit.key].completed)) s[r.unit.key] = true
                        page.selection = s
                    }
                }
                ScButton {
                    visible: !page.local
                    compact: true
                    variant: "primary"
                    iconName: "download"
                    text: qsTr("Download")
                    enabled: parent.count > 0
                    onClicked: { page.download(page.selectedUnits()); page.endSelection() }
                }
                ScButton {
                    compact: true
                    iconName: "eye"
                    text: qsTr("Mark read")
                    enabled: parent.count > 0
                    onClicked: { Library.markUnits(page.series, page.selectedUnits(), true); page.endSelection() }
                }
                ScButton {
                    compact: true
                    iconName: "eye-off"
                    text: qsTr("Mark unread")
                    enabled: parent.count > 0
                    onClicked: { Library.markUnits(page.series, page.selectedUnits(), false); page.endSelection() }
                }
                ScButton {
                    compact: true
                    iconName: "bookmark-plus"
                    text: qsTr("Bookmark")
                    enabled: parent.count > 0
                    onClicked: {
                        const keys = page.selectedUnits().map(c => c.key)
                        // All bookmarked already: the button takes them off.
                        Library.setBookmarked(page.seriesId, keys, !keys.every(k => page.bookmarks[k]))
                        page.endSelection()
                    }
                }
                ScButton {
                    visible: !page.local
                    compact: true
                    iconName: "trash"
                    text: qsTr("Remove downloads")
                    enabled: page.selectedUnits().some(c => page.downloads[c.key] && page.downloads[c.key].status === "done")
                    onClicked: {
                        for (const c of page.selectedUnits())
                            if (page.downloads[c.key] && page.downloads[c.key].status === "done") Downloads.removeUnit(page.seriesId, c.key)
                        page.endSelection()
                    }
                }
            }

            // On phones the chapter search gets its own row.
            Item {
                visible: Theme.compact && page.tab === "chapters" && page.chapters.length > 10
                width: list.width
                height: visible ? 50 : 0
                OutsideList {
                    view: list
                    outside: page
                    x: Theme.gutter
                    width: parent.width - 2 * Theme.gutter
                    height: 40
                    visible: parent.visible
                    SearchField {
                        width: parent.width
                        implicitHeight: 40
                        placeholderText: qsTr("Find chapter")
                        onTextChanged: page.filter = text
                    }
                }
            }

            StateView {
                width: list.width
                height: visible ? 220 : 0
                // Only on the chapters tab (StateView shows itself when needed).
                task: page.tab === "chapters" ? page.task : null
                empty: page.tab === "chapters" && page.task && page.task.ok && (page.chapters.length === 0 || page.rows.length === 0)
                emptyTitle: page.chapters.length > 0 ? qsTr("No chapters match") : qsTr("No chapters available")
                emptyText: page.chapters.length > 0 ? (page.filtersOn > 0 ? qsTr("Your filters hide every chapter. Clear them with the funnel above.") : qsTr("Nothing matches that search."))
                                                    : qsTr("This source has not published readable chapters for this title.")
                onRetry: page.task = page.load()
            }
            Text {
                visible: page.task && page.task.ok && page.task.result.offline === true
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                bottomPadding: 10
                text: qsTr("Showing the saved chapter list. ") + (page.task && page.task.ok ? (page.task.result.error || "") : "")
                color: Theme.warning
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                wrapMode: Text.Wrap
            }
        }

        delegate: UnitRow {
            id: unitRow
            required property var modelData
            width: list.width
            unit: modelData.unit
            progress: page.progress[modelData.unit.key] || null
            download: page.downloads[modelData.unit.key] || null
            canDownload: !page.local
            current: page.resumeInfo.index === modelData.index
            selecting: page.selecting
            selected: page.selection[modelData.unit.key] === true
            bookmarked: page.bookmarks[modelData.unit.key] === true
            onMenuRequested: (x, y) => page.showMenu(unitRow, x, y, unitRow.modelData.unit, unitRow.modelData.index)
            onClicked: {
                if (page.selecting) {
                    const s = Object.assign({}, page.selection)
                    if (s[modelData.unit.key]) delete s[modelData.unit.key]
                    else s[modelData.unit.key] = true
                    page.selection = s
                } else {
                    page.openAt(modelData.index)
                }
            }
            onToggleRead: Library.markUnits(page.series, [modelData.unit], !(progress && progress.completed))
            onRequestDownload: page.download([modelData.unit])
            onRemoveDownload: Downloads.removeUnit(page.seriesId, modelData.unit.key)
        }

        footer: Item {
            width: list.width
            height: pager.visible ? 64 : 0
            Row {
                id: pager
                visible: page.tab === "chapters" && page.pageCount > 1
                anchors.centerIn: parent
                spacing: 4
                IconButton {
                    iconName: "chevron-left"
                    enabled: page.pageIndex > 0
                    opacity: enabled ? 1 : 0.35
                    tip: qsTr("Previous page")
                    onClicked: page.pageIndex--
                }
                Repeater {
                    // The first and last pages and those around the current one.
                    model: {
                        const n = page.pageCount, p = page.pageIndex
                        const set = new Set([0, n - 1, p - 1, p, p + 1])
                        if (!Theme.compact) { set.add(p - 2); set.add(p + 2) }
                        const pages = [...set].filter(i => i >= 0 && i < n).sort((a, b) => a - b)
                        const out = []
                        for (let i = 0; i < pages.length; ++i) {
                            if (i > 0 && pages[i] - pages[i - 1] > 1) out.push(-1)
                            out.push(pages[i])
                        }
                        return out
                    }
                    Rectangle {
                        required property int modelData
                        width: modelData < 0 ? 20 : Math.max(34, label.implicitWidth + 16)
                        height: 34
                        radius: 17
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData === page.pageIndex ? Theme.accent : pageHover.hovered ? Theme.surface2 : "transparent"
                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: parent.modelData < 0 ? "…" : parent.modelData + 1
                            color: parent.modelData === page.pageIndex ? Theme.accentInk : Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textSm
                            font.features: { "tnum": 1 }
                        }
                        HoverHandler { id: pageHover; enabled: parent.modelData >= 0 }
                        TapHandler {
                            enabled: parent.modelData >= 0
                            onTapped: page.pageIndex = parent.modelData
                        }
                    }
                }
                IconButton {
                    iconName: "chevron-right"
                    enabled: page.pageIndex < page.pageCount - 1
                    opacity: enabled ? 1 : 0.35
                    tip: qsTr("Next page")
                    onClicked: page.pageIndex++
                }
            }
        }

        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
        }
    }

    BackToTop { flick: list }
}
