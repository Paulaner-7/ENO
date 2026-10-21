import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Anime details: AniList metadata, episodes with thumbnails and progress,
// related entries and recommendations.
Item {
    id: page
    property int anilistId: 0
    property var summary: ({})
    property bool autoResume: false
    property string url: ""

    property var mediaTask: Anime.media(anilistId)
    property var episodesTask: Anime.episodes(anilistId)
    property var recsTask: Anime.recommendations(anilistId)

    readonly property var media: mediaTask && mediaTask.ok ? mediaTask.result : Object.assign({}, summary, { anilistId: anilistId })
    readonly property var series: Anime.seriesFor(media)
    readonly property string seriesId: Anime.seriesId(anilistId)
    readonly property var episodes: episodesTask && episodesTask.ok ? episodesTask.result : []
    readonly property var progress: { Library.revision; return Library.progressMap(seriesId) }
    readonly property bool saved: { Library.revision; return Library.isSaved(seriesId) }
    property bool descending: false
    // Episodes, Reviews or Discussion.
    property string tab: "episodes"
    // Long series are shown 100 episodes at a time.
    property int range: 0
    readonly property int rangeSize: 100
    readonly property var visibleEpisodes: {
        const list = descending ? episodes.slice().reverse() : episodes
        return list.slice(range * rangeSize, (range + 1) * rangeSize)
    }
    onEpisodesChanged: {
        const i = resumeInfo.index
        range = Math.max(0, Math.floor((descending ? episodes.length - 1 - i : i) / rangeSize))
    }
    onDescendingChanged: range = 0

    readonly property var resumeInfo: {
        Library.revision
        if (episodes.length === 0) return { index: 0, label: qsTr("Watch"), detail: "" }
        const last = Library.lastProgress(seriesId)
        if (last && last.unitKey) {
            const i = episodes.findIndex(e => e.key === last.unitKey)
            if (i >= 0) {
                const reallyFinished = last.completed && (last.percent >= 0.96 || (last.position && last.position.duration > 0 && last.position.seconds >= last.position.duration - 20))
                if (reallyFinished && i + 1 < episodes.length)
                    return { index: i + 1, label: qsTr("Watch episode ") + (i + 2), detail: "" }
                return { index: i, label: qsTr("Continue episode ") + (i + 1), detail: last.percent > 0.02 ? Math.round(last.percent * 100) + qsTr("% watched") : "" }
            }
        }
        return { index: 0, label: qsTr("Watch episode 1"), detail: "" }
    }

    function watch(i) {
        if (i < 0 || i >= episodes.length) return
        Library.markSeen(seriesId)
        Router.openWatch(media, episodes, i)
    }

    property string relatedFilter: "TV"
    readonly property var allRelations: (page.media.relations || []).map(r => Object.assign({ media: "anime", id: Anime.seriesId(r.anilistId) }, r))
    readonly property var relatedFilterOptions: ["TV", "Movie", "OVA", "Special", "All"]

    function matchFormat(fmt, category) {
        if (!fmt) return false
        const f = String(fmt).toUpperCase().replace(/_/g, " ").trim()
        if (category === "TV") return f === "TV" || f === "TV SHORT"
        if (category === "Movie") return f === "MOVIE"
        if (category === "OVA") return f === "OVA" || f === "ONA"
        if (category === "Special") return f === "SPECIAL"
        return true
    }

    readonly property var filteredRelations: {
        if (relatedFilter === "All") return allRelations
        return allRelations.filter(r => page.matchFormat(r.format, page.relatedFilter))
    }

    onAllRelationsChanged: {
        if (allRelations.length > 0) {
            const hasTv = allRelations.some(r => page.matchFormat(r.format, "TV"))
            relatedFilter = hasTv ? "TV" : "All"
        }
    }

    Component.onCompleted: {
        if (page.autoResume && page.episodesTask && page.episodesTask.ok) {
            page.autoResume = false
            page.watch(page.resumeInfo.index)
        }
    }

    Connections {
        target: page.episodesTask
        function onFinished() {
            if (page.autoResume) {
                page.autoResume = false
                page.watch(page.resumeInfo.index)
            }
        }
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 30

            DetailsHeader {
                id: header
                series: Object.assign({}, page.media, { author: (page.media.studios || []).join(", ") })
                anilist: page.media
                kicker: qsTr("Anime") + (page.media.format ? " · " + page.media.format : "")
                meta: [
                    page.media.status,
                    page.media.year,
                    page.media.episodes ? page.media.episodes + qsTr(" episodes") : (page.media.released ? page.media.released + qsTr(" episodes so far") : ""),
                    page.media.duration ? page.media.duration + qsTr(" min") : "",
                    page.media.score ? "★ " + page.media.score : "",
                    page.media.nextEpisode ? qsTr("Ep ") + page.media.nextEpisode + " " + App.relativeTime(page.media.nextAiringAt) : ""
                ]
                primaryText: page.resumeInfo.label
                primaryDetail: page.resumeInfo.detail
                busy: page.episodesTask ? page.episodesTask.loading : false
                saved: page.saved
                onPrimary: page.watch(page.resumeInfo.index)
                onToggleSaved: {
                    if (page.saved) Library.remove(page.seriesId)
                    else Library.save(page.series)
                    Router.toast(page.saved ? qsTr("Added to your library") : qsTr("Removed from your library"), page.saved ? "success" : "info")
                }
                IconButton {
                    visible: !!page.media.trailer
                    iconName: "circle-play"
                    filled: true
                    tip: qsTr("Watch trailer")
                    onClicked: App.openUrl(page.media.trailer)
                }
                IconButton {
                    iconName: "external-link"
                    filled: true
                    tip: qsTr("Open on AniList")
                    onClicked: App.openUrl("https://anilist.co/anime/" + page.anilistId)
                }
                IconButton {
                    visible: !!Trackers.anilist.name || !!Trackers.mal.name || !!Trackers.comick.name
                    iconName: "link"
                    filled: true
                    tip: qsTr("Tracking")
                    onClicked: tracking.open()
                }
                Component.onCompleted: Qt.callLater(() => Router.shell.land(header.coverItem))
            }

            Item {
                width: parent.width
                height: 34
                visible: page.episodes.length > 0
                Text {
                    x: Theme.gutter
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Episodes")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textXl
                    font.weight: Font.DemiBold
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.gutter - 6
                    anchors.verticalCenter: parent.verticalCenter
                    IconButton {
                        iconName: "check"
                        tip: qsTr("Mark all as watched")
                        onClicked: Library.markUnits(page.series, page.episodes, true)
                    }
                    IconButton {
                        iconName: "arrow-down-wide-narrow"
                        rotation: page.descending ? 0 : 180
                        tip: page.descending ? qsTr("Newest first") : qsTr("Oldest first")
                        onClicked: page.descending = !page.descending
                        Behavior on rotation { NumberAnimation { duration: Motion.normal } }
                    }
                }
            }

            Flow {
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 8
                visible: page.episodes.length > page.rangeSize
                Repeater {
                    model: Math.ceil(page.episodes.length / page.rangeSize)
                    Chip {
                        required property int index
                        readonly property int first: page.descending ? page.episodes.length - index * page.rangeSize : index * page.rangeSize + 1
                        readonly property int last: page.descending ? Math.max(1, first - page.rangeSize + 1) : Math.min(page.episodes.length, first + page.rangeSize - 1)
                        text: first + "–" + last
                        selected: page.range === index
                        onClicked: page.range = index
                    }
                }
            }

            Flow {
                id: episodeFlow
                visible: true
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 16
                readonly property int columns: Math.max(2, Math.floor((width + spacing) / (250 + spacing)))
                readonly property real cardWidth: (width - (columns - 1) * spacing) / columns
                Repeater {
                    model: page.visibleEpisodes
                    delegate: AbstractButton {
                        id: ep
                        required property var modelData
                        required property int index
                        readonly property var state: page.progress[modelData.key] || null
                        width: episodeFlow.cardWidth
                        height: width * 9 / 16 + 54
                        hoverEnabled: true
                        // By key: a Repeater hands its delegates copies of the
                        // model's objects, so indexOf(modelData) is always -1.
                        onClicked: page.watch(page.episodes.findIndex(e => e.key === modelData.key))
                        background: null
                        contentItem: Column {
                            spacing: 8
                            Item {
                                width: ep.width
                                height: width * 9 / 16
                                Cover {
                                    anchors.fill: parent
                                    source: ep.modelData.image || page.media.banner || page.media.cover || ""
                                    title: ep.modelData.title
                                    zoom: ep.hovered ? 1.05 : 1
                                    Behavior on zoom { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.radiusMd
                                    color: "black"
                                    opacity: ep.hovered ? 0.35 : (ep.state && ep.state.completed ? 0.45 : 0)
                                    Behavior on opacity { NumberAnimation { duration: Motion.normal } }
                                }
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 46
                                    height: 46
                                    radius: 23
                                    color: Theme.alpha("#ffffff", 0.92)
                                    scale: ep.hovered ? 1 : 0.6
                                    opacity: ep.hovered ? 1 : 0
                                    Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutBack } }
                                    Behavior on opacity { NumberAnimation { duration: Motion.fast } }
                                    Icon { anchors.centerIn: parent; anchors.horizontalCenterOffset: 2; name: "play"; size: 20; color: "#111" }
                                }
                                Rectangle {
                                    x: 8; y: 8
                                    height: 22
                                    width: epNum.implicitWidth + 14
                                    radius: 6
                                    color: Theme.alpha("#000000", 0.65)
                                    Text { id: epNum; anchors.centerIn: parent; text: "EP " + ep.modelData.number; color: "white"; font.family: Theme.fontSans; font.pixelSize: 11; font.weight: Font.Bold }
                                }
                                Rectangle {
                                    visible: ep.state !== null
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 8
                                    height: 4
                                    radius: 2
                                    color: Theme.alpha("#000000", 0.5)
                                    Rectangle {
                                        width: parent.width * (ep.state ? (ep.state.completed ? 1 : Math.max(0.04, ep.state.percent)) : 0)
                                        height: parent.height
                                        radius: 2
                                        color: "white"
                                    }
                                }
                            }
                            Text {
                                width: ep.width
                                text: ep.modelData.title
                                color: Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                width: ep.width
                                text: ep.modelData.airDate ? App.relativeTime(ep.modelData.airDate) : ""
                                color: Theme.textFaint
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textXs
                            }
                        }
                    }
                }
            }

            StateView {
                width: parent.width
                height: visible ? 220 : 0
                task: page.episodesTask
                empty: page.episodesTask && page.episodesTask.ok && page.episodes.length === 0
                emptyTitle: qsTr("No episodes yet")
                emptyText: page.media.nextEpisode ? qsTr("Episode ") + page.media.nextEpisode + qsTr(" airs ") + App.relativeTime(page.media.nextAiringAt) + "." : qsTr("This anime has not aired yet.")
                emptyIcon: "calendar"
                onRetry: page.episodesTask = Anime.episodes(page.anilistId)
            }

            Shelf {
                title: qsTr("Related")
                visible: page.allRelations.length > 0
                headerExtra: Component {
                    ChipRow {
                        Repeater {
                            model: page.relatedFilterOptions
                            Chip {
                                required property var modelData
                                text: modelData
                                selected: page.relatedFilter === modelData
                                onClicked: page.relatedFilter = modelData
                            }
                        }
                    }
                }
                items: page.filteredRelations
                subtitleFor: (it) => [it.relation, it.format, it.year].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
            Shelf {
                title: qsTr("You might also like")
                items: page.recsTask && page.recsTask.ok ? page.recsTask.result : []
                subtitleFor: (it) => [it.format, it.year].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
        }
    }

    BackToTop { flick: flick }

    TrackingDialog {
        id: tracking
        seriesId: page.seriesId
        media: "anime"
        seriesTitle: page.media.title || ""
    }
}
