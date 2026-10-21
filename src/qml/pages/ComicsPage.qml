import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Comic search across every source, plus the latest chapters when idle.
Item {
    id: page
    property string query: ""
    property string source: App.settings.comicSource || "all"
    property var searchTask: null
    // The results so far: sources answer one by one, so the grid fills
    // while the slower ones are still searching.
    readonly property var found: !searchTask ? null : searchTask.ok ? searchTask.result : searchTask.loading ? (searchTask.partial || null) : null
    readonly property int stillSearching: found && searchTask.loading ? (found.pending || 0) : 0
    property int latestLimit: 36
    property var latestTask: Comics.latest(latestLimit)
    property var historyTask: Library.continueItems("comic", 16)
    // The shelves of the chosen comic catalogue, when search goes through it.
    property var feedsTask: useCatalog ? Comics.catalogFeeds(App.settings.showNsfwSources === true) : null
    readonly property var feeds: feedsTask && feedsTask.ok ? feedsTask.result : ({ shelves: [] })
    // The catalogue's name, for messages.
    readonly property string catalogLabel: feedsTask && feedsTask.ok && feedsTask.result.label ? feedsTask.result.label
        : App.settings.catalogComic === "anilist" ? "AniList" : App.settings.catalogComic === "mal" ? "MyAnimeList" : qsTr("Comick")
    property var history: []
    Connections {
        target: page.historyTask
        function onFinished(result) { page.history = Router.keep(page.history, result) }
    }
    Connections {
        target: Library
        function onChanged() { refreshHistory.restart() }
    }
    Timer { id: refreshHistory; interval: 500; onTriggered: page.historyTask = Library.continueItems("comic", 16) }
    // Files on this device are listed without a search.
    // "All sources" searches the chosen comic catalogue, then finds the pick
    // on a source.
    readonly property bool useCatalog: source === "all" && (App.settings.catalogComic || "comick") !== "none"
        && (App.settings.catalogComic !== "mal" || Trackers.malAvailable)
    // The catalogue could not be reached, so the sources were searched instead.
    property bool catalogFailed: false
    readonly property bool catalogResults: !!searchTask && searchTask.ok && !catalogFailed && useCatalog && query.length > 0
    readonly property bool browsingLocal: source === "local" && query.length === 0
    readonly property bool searching: !editing && (query.length > 0 || source === "local")
    // Edit page: arrange the sections (PageLayout).
    property bool editing: false
    PageArranger { id: topLayout; page: "comics-top"; column: column; editing: page.editing; defaults: ["search", "sources"] }
    PageArranger {
        id: idleLayout
        page: "comics"
        column: idle
        editing: page.editing
        defaults: ["recent", "continue", "trending", "popular", "newest", "completed", "top", "manhwa", "manhua", "latest"]
    }
    Component.onCompleted: if (source === "local") run("")

    function search(text) {
        field.text = text
        run(text)
    }

    function run(text) {
        const trimmed = text.trim()
        if (Router.takeRepositoryLink(trimmed)) return
        // A pasted series link opens that series directly.
        if (/^https?:\/\//.test(trimmed) && Comics.sourceForUrl(trimmed)) {
            Router.openSeries({ media: "comic", sourceUrl: trimmed, title: "", source: Comics.sourceForUrl(trimmed) }, null)
            return
        }
        query = trimmed
        if (query.length > 0) recent.remember(query)
        catalogFailed = false
        searchTask = query.length > 0 && useCatalog ? Comics.catalog(query)
            : query.length > 0 || source === "local" ? Comics.search(query, source) : null
        flick.scrollTo(0)
    }

    Connections {
        target: page.searchTask
        function onFailed() {
            if (!page.useCatalog || page.catalogFailed || page.query.length === 0) return
            page.catalogFailed = true
            page.searchTask = Comics.search(page.query, "all")
        }
    }

    ReadFromDialog { id: readFrom }

    // Each source's entry in the running search, by key, for the chips.
    readonly property var providerMap: {
        const map = {}
        for (const p of (page.found && page.found.providers) || []) map[p.source] = p
        return map
    }
    readonly property var sources: Comics.sources().filter(s => s.enabled && (!s.nsfw || App.settings.showNsfwSources))

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: Math.round(Theme.sectionGap * 0.72)

            PageHeader {
                title: qsTr("Comics")
                subtitle: page.sources.length + qsTr(" sources · manhwa, manga and webtoons")
                IconButton {
                    iconName: "layout-dashboard"
                    tip: qsTr("Edit page")
                    active: page.editing
                    onClicked: page.editing = !page.editing
                }
                ScButton {
                    text: qsTr("Extensions")
                    iconName: "puzzle"
                    onClicked: Router.openExtensions()
                    // A dot when updates wait.
                    Rectangle {
                        visible: Extensions.updateCount > 0
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 4
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.accent
                    }
                }
            }

            PageSection {
                arranger: topLayout
                key: "search"
                label: qsTr("Search bar")
                SearchField {
                    id: field
                    x: Theme.gutter
                    width: Math.min(parent.width - 2 * Theme.gutter, 760)
                    large: true
                    placeholderText: page.useCatalog ? qsTr("Search every manhwa, manga and comic") : qsTr("Search titles across every source")
                    onSubmitted: (text) => page.run(text)
                    onTextChanged: if (text.length === 0 && page.query.length > 0) page.run("")
                }
            }

            PageSection {
                arranger: topLayout
                key: "sources"
                label: qsTr("Source chips")
                SourceChips {
                    sources: page.sources
                    current: page.source
                    providers: page.providerMap
                    showLanguage: true
                    onPicked: (key) => {
                        page.source = key
                        App.setSetting("comicSource", key)
                        if (key === "all" || page.searching || key === "local") page.run(page.query)
                    }
                }
            }

            // ---------------------------------------------------- results
            Column {
                visible: page.searching
                width: parent.width
                spacing: 18

                Item {
                    width: parent.width
                    height: 24
                    visible: !!page.found
                    Text {
                        x: Theme.gutter
                        text: !page.found ? ""
                            : page.browsingLocal ? page.found.comics.length + qsTr(" on this device")
                            : page.catalogResults ? qsTr("%1 titles for “%2” from %3's catalogue").arg(page.found.comics.length).arg(page.query).arg(page.catalogLabel)
                            : page.found.comics.length + qsTr(" results for “") + page.query + "”"
                                + (page.catalogFailed ? qsTr(" · %1 is unavailable, so your sources were searched").arg(page.catalogLabel) : "")
                                + (page.stillSearching ? (page.stillSearching === 1 ? qsTr(" · 1 source still searching") : qsTr(" · %1 sources still searching").arg(page.stillSearching)) : "")
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.gutter
                        readonly property var failed: page.found ? page.found.providers.filter(p => !p.ok && !p.pending) : []
                        visible: failed.length > 0
                        text: (failed.length === 1 ? qsTr("%1 is unavailable") : qsTr("%1 are unavailable")).arg(failed.map(p => p.label).join(", "))
                        color: Theme.warning
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        MouseArea {
                            id: failedHover
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                        ToolTip.visible: failedHover.containsMouse
                        ToolTip.text: failed.map(p => p.label + ": " + p.error).join("\n")
                    }
                }

                Flow {
                    x: Theme.gutter
                    width: parent.width - 2 * Theme.gutter
                    spacing: 8
                    Repeater {
                        model: page.found ? page.found.providers.filter(p => p.verifyUrl) : []
                        ScButton {
                            required property var modelData
                            compact: true
                            iconName: "globe"
                            text: qsTr("Pass %1's browser check").arg(modelData.label)
                            onClicked: Router.openSiteCheck(modelData.verifyUrl)
                        }
                    }
                }

                CoverGrid {
                    items: page.found ? page.found.comics : []
                    subtitleFor: (it) => it.catalog
                        ? [it.type, it.status, it.lastChapter ? qsTr("Ch. ") + it.lastChapter : ""].filter(s => s).join(" · ")
                        : it.sourceLabel + (it.type ? " · " + it.type : "")
                    onOpened: (it, cover) => it.catalog ? readFrom.start(it) : Router.openSeries(it, cover)
                }

                StateView {
                    width: parent.width
                    task: page.searchTask
                    empty: page.searchTask && page.searchTask.ok && page.found.comics.length === 0
                    emptyTitle: page.browsingLocal ? qsTr("No comics on this device yet") : qsTr("No comics found")
                    emptyText: page.browsingLocal
                        ? qsTr("Choose the folders that hold your comics in Settings → Sources. Folders of images, CBZ, CBR and PDF files all work.")
                        : qsTr("Try a shorter title or another source. You can also paste a series link.")
                    emptyIcon: page.browsingLocal ? "folder-open" : "search"
                    onRetry: page.run(page.query)
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: page.browsingLocal && page.searchTask && page.searchTask.ok && page.found.comics.length === 0
                    text: qsTr("Choose folders")
                    iconName: "folder-open"
                    onClicked: Router.openSettings("sources")
                }
            }

            // ---------------------------------------------------- idle
            Column {
                id: idle
                visible: !page.searching
                width: parent.width
                spacing: Theme.sectionGap

                PageSection {
                    arranger: idleLayout
                    key: "recent"
                    label: qsTr("Recent searches")
                    RecentSearches {
                        id: recent
                        key: "recentComicSearches"
                        onPicked: (text) => page.search(text)
                    }
                }

                PageSection {
                    arranger: idleLayout
                    key: "continue"
                    label: qsTr("Continue reading")
                    Shelf {
                        title: qsTr("Continue reading")
                        items: page.history
                        subtitleFor: (it) => it.unitTitle || ""
                        progressFor: (it) => it.completed ? 1 : (it.percent || 0)
                        onOpened: (it, cover) => Router.resume(it)
                    }
                }

                // While the catalogue loads (or fails), one shelf shows that.
                Shelf {
                    width: parent.width
                    visible: page.useCatalog && !(page.feedsTask && page.feedsTask.ok)
                    title: qsTr("Trending this week")
                    items: []
                    loading: page.feedsTask ? page.feedsTask.loading : false
                    error: page.feedsTask && !page.feedsTask.loading ? page.feedsTask.error : ""
                    onRetry: page.feedsTask = Comics.catalogFeeds(App.settings.showNsfwSources === true)
                }
                Repeater {
                    model: page.useCatalog ? page.feeds.shelves : []
                    PageSection {
                        id: feed
                        required property var modelData
                        arranger: idleLayout
                        key: modelData.key
                        label: Router.shelfTitle(modelData.key)
                        Shelf {
                            width: parent.width
                            title: Router.shelfTitle(feed.modelData.key)
                            items: feed.modelData.items
                            subtitleFor: (it) => [it.type, it.lastChapter ? qsTr("Ch. ") + it.lastChapter : ""].filter(s => s).join(" · ")
                            onOpened: (it, cover) => readFrom.start(it)
                        }
                    }
                }

                PageSection {
                    arranger: idleLayout
                    key: "latest"
                    label: qsTr("Latest chapters")
                  Column {
                    width: parent.width
                    spacing: 16
                    Text {
                        x: Theme.gutter
                        text: qsTr("Latest chapters")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textXl
                        font.weight: Font.DemiBold
                    }
                    CoverGrid {
                        items: page.latestTask && page.latestTask.ok ? page.latestTask.result.updates : []
                        subtitleFor: (it) => (it.chapterTitle || "") + (it.publishedAt ? " · " + App.relativeTime(it.publishedAt) : "")
                        onOpened: (it, cover) => Router.openSeries(it, cover)
                    }
                    StateView {
                        width: parent.width
                        task: page.latestTask
                        onRetry: page.latestTask = Comics.latest(page.latestLimit)
                    }
                    ScButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: page.latestTask && page.latestTask.ok && page.latestLimit < 120
                        text: qsTr("Show more")
                        onClicked: {
                            page.latestLimit += 36
                            page.latestTask = Comics.latest(page.latestLimit)
                        }
                    }
                  }
                }
            }
        }
    }

    BackToTop { flick: flick }
    EditPageBar {
        editing: page.editing
        arrangers: [topLayout, idleLayout]
        onDone: page.editing = false
    }
}
