import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Everything at a glance: continue where you left off, library updates and
// what is new across comics, novels and anime (in ENO, anime only).
Item {
    id: page

    // ENO: anime only, so Home shows (and loads) nothing else.
    readonly property string onlyMedia: Router.animeOnly ? "anime" : ""
    property var historyTask: Library.continueItems(onlyMedia, 24)
    property var libraryTask: Library.entries(onlyMedia)
    // A section removed in Edit page loads nothing.
    readonly property bool comicsOn: Router.mediaAllowed("comic") && (PageLayout.revision, !PageLayout.isRemoved("home", "comics"))
    readonly property bool animeOn: (PageLayout.revision, !PageLayout.isRemoved("home", "anime"))
    readonly property bool novelsOn: Router.mediaAllowed("novel") && (PageLayout.revision, !PageLayout.isRemoved("home", "novels"))
    // Only a change to the added shelves rebuilds them (not moving one).
    readonly property string extrasJson: (PageLayout.revision, JSON.stringify(PageLayout.extras("home")))
    property var latestTask: comicsOn ? Comics.latest(30) : null
    property var animeTask: animeOn ? Anime.home() : null
    property var novelsTask: novelsOn ? Novels.discover() : null

    // The last results, kept while a refresh runs.
    property var history: []
    property var library: []
    readonly property var updates: library.filter(e => e.newUnits > 0)
    Connections {
        target: page.historyTask
        function onFinished(result) { page.history = Router.keep(page.history, result) }
    }
    Connections {
        target: page.libraryTask
        function onFinished(result) { page.library = Router.keep(page.library, result) }
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
    Timer {
        id: refresh
        interval: 400
        onTriggered: {
            page.historyTask = Library.continueItems(page.onlyMedia, 24)
            page.libraryTask = Library.entries(page.onlyMedia)
            if (!Router.animeOnly) Story.refreshCards()
        }
    }

    ReadFromDialog { id: storyReadFrom }
    Component.onCompleted: if (!Router.animeOnly) Story.refreshCards()

    // Edit page: arrange the sections (PageLayout).
    property bool editing: false
    PageArranger {
        id: layout
        page: "home"
        column: column
        editing: page.editing
        removable: true
        defaults: ["greeting", "resume", "continue", "story", "updates", "comics", "anime", "novels"]
    }

    function greeting() {
        const h = new Date().getHours()
        return h < 5 ? qsTr("Up late") : h < 12 ? qsTr("Good morning") : h < 18 ? qsTr("Good afternoon") : qsTr("Good evening")
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: Theme.sectionGap

            PageSection {
                arranger: layout
                key: "greeting"
                label: qsTr("Greeting")
                Column {
                    width: parent.width
                    spacing: 18
                    Item {
                        width: parent.width
                        height: Math.max(heading.implicitHeight, quickLinks.implicitHeight) + 16
                        Column {
                            id: heading
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.gutter
                            anchors.right: quickLinks.left
                            anchors.rightMargin: 16
                            y: 12
                            spacing: 6
                            Text {
                                text: Qt.formatDate(new Date(), "dddd, d MMMM")
                                color: Theme.textFaint
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                                font.weight: Font.Medium
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 1.2
                                horizontalAlignment: Text.AlignLeft
                            }
                            Text {
                                width: parent.width
                                text: page.greeting()
                                color: Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: Theme.compact ? 32 : 42
                                font.weight: Font.Bold
                                font.letterSpacing: -1
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignLeft
                            }
                        }
                        // Quick links and notification bell.
                        Row {
                            id: quickLinks
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.gutter
                            anchors.bottom: heading.bottom
                            anchors.bottomMargin: 4
                            spacing: 10
                            ScButton {
                                visible: !Theme.compact
                                text: qsTr("Anime")
                                iconName: "tv"
                                glassBezel: true
                                onClicked: Router.go("anime")
                            }
                            ScButton {
                                visible: !Theme.compact && Router.animeOnly
                                text: qsTr("Calendario")
                                iconName: "calendar"
                                glassBezel: true
                                onClicked: Router.openAnimeSchedule()
                            }
                            ScButton {
                                visible: !Theme.compact && Router.sectionAllowed("comics")
                                text: qsTr("Manga")
                                iconName: "book-open"
                                glassBezel: true
                                onClicked: Router.go("comics")
                            }
                            ScButton {
                                visible: !Theme.compact && Router.sectionAllowed("novels")
                                text: qsTr("Novels")
                                iconName: "book-text"
                                glassBezel: true
                                onClicked: Router.go("novels")
                            }
                            Item {
                                width: notifButton.width
                                height: notifButton.height
                                anchors.verticalCenter: parent.verticalCenter
                                IconButton {
                                    id: notifButton
                                    filled: true
                                    iconName: Notifications.unread > 0 ? "bell-dot" : "bell"
                                    tip: qsTr("Notifications")
                                    onClicked: Router.go("notifications")
                                }
                                Rectangle {
                                    visible: Notifications.unread > 0
                                    anchors.top: notifButton.top
                                    anchors.right: notifButton.right
                                    anchors.topMargin: 4
                                    anchors.rightMargin: 4
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: Theme.accent
                                }
                            }
                        }
                    }
                    // December to 7 January: the year's Wrapped.
                    Rectangle {
                        id: wrappedBanner
                        readonly property int year: Router.wrappedYear()
                        visible: year > 0 && Number(App.settings.wrappedDismissed) !== year
                        x: Theme.gutter
                        width: parent.width - 2 * Theme.gutter
                        height: visible ? Math.max(76, bannerText.height + 32) : 0
                        radius: Theme.radiusXl
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: "#3b1d8f" }
                            GradientStop { position: 0.6; color: "#7a1f5c" }
                            GradientStop { position: 1; color: "#b4361f" }
                        }
                        TapHandler { onTapped: Router.openWrapped(wrappedBanner.year) }
                        Image {
                            id: bannerMark
                            x: 18
                            anchors.verticalCenter: parent.verticalCenter
                            source: App.icon("sparkles", "#ffffff")
                            sourceSize: Qt.size(26, 26)
                            width: 26
                            height: 26
                        }
                        Column {
                            id: bannerText
                            anchors.left: bannerMark.right
                            anchors.leftMargin: 14
                            anchors.right: bannerClose.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                width: parent.width
                                wrapMode: Text.Wrap
                                text: qsTr("Your %1 Wrapped is here").arg(wrappedBanner.year)
                                color: "white"
                                font.family: Theme.fontDisplay
                                font.pixelSize: Theme.textXl
                                font.weight: Font.Bold
                            }
                            Text {
                                width: parent.width
                                wrapMode: Text.Wrap
                                text: Router.animeOnly ? qsTr("Your year in anime") : qsTr("Your year in comics, novels and anime")
                                color: Qt.rgba(1, 1, 1, 0.8)
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                            }
                        }
                        IconButton {
                            id: bannerClose
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "x"
                            iconColor: "white"
                            tip: qsTr("Not now")
                            onClicked: App.setSetting("wrappedDismissed", wrappedBanner.year)
                        }
                    }
                }
            }

            // The latest of them, large.
            PageSection {
                arranger: layout
                key: "resume"
                label: Router.animeOnly ? qsTr("Last watched") : qsTr("Last read or watched")
                shown: page.history.length > 0
                Loader {
                    width: parent.width
                    active: page.history.length > 0
                    visible: active
                    sourceComponent: ResumeCard { entry: page.history[0] }
                }
            }

            PageSection {
                arranger: layout
                key: "continue"
                label: qsTr("Continue")
                Shelf {
                    title: qsTr("Continue")
                    items: page.history.slice(1)
                    loading: page.historyTask ? page.historyTask.loading && page.history.length === 0 : false
                    subtitleFor: (it) => it.media === "anime" ? qsTr("Episode ") + (it.unitNumber || it.unitKey) : (it.unitTitle || "")
                    progressFor: (it) => (it.completed && (it.percent >= 0.96 || (it.position && it.position.duration > 0 && it.position.seconds >= it.position.duration - 20))) ? 1 : (it.percent || 0)
                    onOpened: (it, cover) => Router.resume(it)
                }
            }

            // An anime season finished: where its comic or novel goes on.
            PageSection {
                arranger: layout
                key: "story"
                // Where an anime goes on as a comic or novel: not in ENO.
                available: !Router.animeOnly
                label: qsTr("Continue the story")
                shown: Story.cards.length > 0
                Column {
                    width: parent.width
                    spacing: 14
                    visible: Story.cards.length > 0
                    Text {
                        x: Theme.gutter
                        text: qsTr("Continue the story")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textXl
                        font.weight: Font.DemiBold
                    }
                    ListView {
                        WheelScroll {}
                        width: parent.width
                        height: Theme.compact ? 132 : 144
                        orientation: ListView.Horizontal
                        leftMargin: Theme.gutter
                        rightMargin: Theme.gutter
                        spacing: 12
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: Story.cards
                        delegate: StoryCard {
                            required property var modelData
                            entry: modelData
                            onOpen: (e) => {
                                if (e.seriesId) Router.openSeries(Library.seriesRow(e.seriesId), null)
                                else if (e.entry) storyReadFrom.start(e.entry)
                            }
                        }
                    }
                }
            }

            PageSection {
                arranger: layout
                key: "updates"
                label: qsTr("New in your library")
                Shelf {
                    title: qsTr("New in your library")
                    items: page.updates
                    badgeFor: (it) => "+" + it.newUnits
                    subtitleFor: (it) => it.latestTitle || ""
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
            }

            PageSection {
                arranger: layout
                key: "comics"
                available: Router.mediaAllowed("comic")
                label: qsTr("Latest comic chapters")
                Shelf {
                    title: qsTr("Latest comic chapters")
                    actionText: qsTr("More")
                    onAction: Router.go("comics")
                    items: page.latestTask && page.latestTask.ok ? page.latestTask.result.updates : []
                    loading: page.latestTask ? page.latestTask.loading : false
                    error: page.latestTask && !page.latestTask.loading ? page.latestTask.error : ""
                    onRetry: page.latestTask = Comics.latest(30)
                    subtitleFor: (it) => (it.chapterTitle || "") + (it.publishedAt ? " · " + App.relativeTime(it.publishedAt) : "")
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
            }

            PageSection {
                arranger: layout
                key: "anime"
                label: qsTr("Trending anime")
                Shelf {
                    title: qsTr("Trending anime")
                    actionText: qsTr("Browse")
                    onAction: Router.go("anime")
                    items: page.animeTask && page.animeTask.ok ? page.animeTask.result.trending : []
                    loading: page.animeTask ? page.animeTask.loading : false
                    error: page.animeTask && !page.animeTask.loading ? page.animeTask.error : ""
                    onRetry: page.animeTask = Anime.home()
                    subtitleFor: (it) => [it.format, it.year].filter(Boolean).join(" · ")
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
            }

            PageSection {
                arranger: layout
                key: "novels"
                available: Router.mediaAllowed("novel")
                label: qsTr("Novels")
                Column {
                    readonly property bool hasContent: page.novelsTask !== null
                    width: parent.width
                    spacing: Theme.sectionGap
                    Repeater {
                        model: page.novelsTask && page.novelsTask.ok ? page.novelsTask.result.shelves.slice(0, 2) : []
                        delegate: Shelf {
                            required property var modelData
                            title: modelData.title + qsTr(" novels")
                            subtitle: modelData.source
                            items: modelData.books
                            subtitleFor: (it) => it.author || it.genres.slice(0, 2).join(", ")
                            onOpened: (it, cover) => Router.openSeries(it, cover)
                        }
                    }
                    Shelf {
                        visible: page.novelsTask && (page.novelsTask.loading || page.novelsTask.error.length > 0)
                        title: qsTr("Novels")
                        loading: page.novelsTask ? page.novelsTask.loading : false
                        error: page.novelsTask && !page.novelsTask.loading ? page.novelsTask.error : ""
                        onRetry: page.novelsTask = Novels.discover()
                    }
                }
            }

            // Shelves of an extension's own listings, added in Edit page.
            Repeater {
                model: JSON.parse(page.extrasJson)
                PageSection {
                    id: extraSection
                    required property var modelData
                    arranger: layout
                    key: modelData.key
                    label: modelData.label
                    available: Router.mediaAllowed(modelData.media)
                    readonly property var service: modelData.media === "anime" ? Anime : modelData.media === "novel" ? Novels : Comics
                    property var task: available ? service.browse(modelData.source, modelData.listing, 1) : null
                    Shelf {
                        title: extraSection.modelData.label
                        items: extraSection.task && extraSection.task.ok ? extraSection.task.result.items : []
                        loading: extraSection.task ? extraSection.task.loading : false
                        error: extraSection.task && !extraSection.task.loading ? extraSection.task.error : ""
                        actionText: qsTr("More")
                        onAction: Router.openSource(extraSection.modelData.source)
                        onRetry: extraSection.task = extraSection.service.browse(extraSection.modelData.source, extraSection.modelData.listing, 1)
                        onOpened: (it, cover) => Router.openSeries(it, cover)
                    }
                }
            }
        }
    }

    BackToTop { flick: flick }
    EditPageBar {
        editing: page.editing
        arrangers: [layout]
        canAdd: true
        onDone: page.editing = false
        onAdd: addSection.open()
    }
    AddSectionSheet { id: addSection; arranger: layout }
}
