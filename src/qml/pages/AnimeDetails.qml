import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Anime details: AniList metadata, episodes with thumbnails and progress,
// related entries and recommendations. An anime opened from an extension's
// source (sourceUrl) shows what that source lists instead: its details, its
// episodes, or its seasons.
Item {
    id: page
    property int anilistId: 0
    property var summary: ({})
    property bool autoResume: false
    property string url: ""
    property string sourceUrl: ""
    readonly property bool fromSource: sourceUrl.length > 0

    // A MyAnimeList title (the anime catalogue) opens as its AniList entry.
    property var malTask: !fromSource && anilistId === 0 && summary.idMal ? Anime.anilistIdFor(summary.idMal) : null
    Connections {
        target: page.malTask
        function onFinished(id) { page.anilistId = id }
    }
    property var mediaTask: fromSource || anilistId === 0 ? null : Anime.media(anilistId)
    property var episodesTask: fromSource ? Anime.sourceAnime(sourceUrl, summary) : anilistId === 0 ? null : Anime.episodes(anilistId)
    property var recsTask: fromSource || anilistId === 0 ? null : Anime.recommendations(anilistId)

    readonly property var media: fromSource ? (episodesTask && episodesTask.ok ? episodesTask.result.series : summary)
                               : mediaTask && mediaTask.ok ? mediaTask.result : Object.assign({ anilistId: anilistId }, summary)
    readonly property var series: fromSource ? media : Anime.seriesFor(media)
    readonly property string seriesId: fromSource ? (summary.id || media.id || "") : Anime.seriesId(anilistId)
    readonly property var episodes: !episodesTask || !episodesTask.ok ? [] : fromSource ? episodesTask.result.episodes : episodesTask.result
    readonly property var seasons: fromSource && episodesTask && episodesTask.ok ? episodesTask.result.seasons : []
    readonly property bool hasSources: { App.dependOn(Extensions.revision); return Anime.sources().some(s => s.enabled) }
    readonly property var progress: { App.dependOn(Library.revision); return Library.progressMap(seriesId) }
    readonly property bool saved: { App.dependOn(Library.revision); return Library.isSaved(seriesId) }
    property bool descending: false
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
        App.dependOn(Library.revision)
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

    // Episodes can be downloaded when they play from an anime source with
    // SCANCORE's own player: {source, animeUrl?}, or null.
    readonly property var downloadSource: {
        App.dependOn(Downloads.revision)
        if (!Anime.nativePlayerAvailable) return null
        if (fromSource) return (media.source || summary.source) ? { source: media.source || summary.source } : null
        const p = Anime.provider(anilistId)
        return p.source ? { source: p.source, animeUrl: p.url } : null
    }
    readonly property var downloadStates: { App.dependOn(Downloads.revision); return seriesId ? Downloads.states(seriesId) : ({}) }
    function download(list) {
        if (!downloadSource || list.length === 0) return
        Downloads.enqueue(Object.assign({}, series, { id: seriesId }), list.map(ep => ({
            key: ep.key, title: ep.title, url: ep.url || "",
            download: Object.assign({}, downloadSource, fromSource ? { episodeUrl: ep.url } : { number: Number(ep.number) })
        })))
        Router.toast(list.length === 1 ? qsTr("Downloading episode %1").arg(list[0].number) : qsTr("Downloading %1 episodes").arg(list.length), "info")
    }
    // Not watched to the end and not downloaded yet.
    function unwatched() {
        return episodes.filter(e => !(progress[e.key] && progress[e.key].completed) && !downloadStates[e.key])
    }
    ActionMenu { id: downloadMenu }

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
                series: Object.assign({}, page.media, { author: page.fromSource ? ((page.media.extra || {}).author || "") : (page.media.studios || []).join(", ") })
                anilist: page.media
                kicker: qsTr("Anime") + (page.fromSource ? " · " + (page.media.sourceLabel || "") : page.media.format ? " · " + page.media.format : "")
                meta: page.fromSource ? [page.media.status, page.seasons.length ? page.seasons.length + qsTr(" seasons") : page.episodes.length ? page.episodes.length + qsTr(" episodes") : ""] : [
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
                    if (!page.saved && !page.series.id) {
                        // A MyAnimeList title is saved as its AniList entry.
                        Router.toast(page.malTask && page.malTask.loading ? qsTr("Still looking this anime up. Try again in a moment.")
                                                                        : qsTr("AniList has no entry for this anime yet, so it cannot be saved."))
                    } else if (page.saved) {
                        Library.remove(page.seriesId)
                        Router.toast(qsTr("Removed from your library"))
                    } else {
                        Library.save(page.series)
                        Router.toast(qsTr("Added to your library"), "success")
                    }
                }
                IconButton {
                    visible: !!page.media.trailer
                    iconName: "circle-play"
                    filled: true
                    tip: qsTr("Watch trailer")
                    onClicked: App.openUrl(page.media.trailer)
                }
                IconButton {
                    visible: !page.fromSource && page.hasSources
                    iconName: "server"
                    filled: true
                    tip: qsTr("Watch from")
                    onClicked: watchFrom.start(page.media, false)
                }
                IconButton {
                    visible: !page.fromSource
                    iconName: "external-link"
                    filled: true
                    tip: qsTr("Open on AniList")
                    onClicked: App.openUrl("https://anilist.co/anime/" + page.anilistId)
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
                        id: downloadButton
                        visible: page.downloadSource !== null
                        iconName: "download"
                        tip: qsTr("Download episodes")
                        onClicked: {
                            const left = page.unwatched()
                            downloadMenu.openAt(downloadButton, 0, downloadButton.height, [
                                { text: qsTr("Next %1 unwatched").arg(Math.min(5, left.length)), icon: "download", enabled: left.length > 0, run: () => page.download(left.slice(0, 5)) },
                                { text: qsTr("All %1 unwatched").arg(left.length), icon: "download", enabled: left.length > 0, run: () => page.download(left) }
                            ])
                        }
                    }
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
                        z: hovered || pressed ? 10 : 0
                        // By key: a Repeater hands its delegates copies of the
                        // model's objects, so indexOf(modelData) is always -1.
                        onClicked: page.watch(page.episodes.findIndex(e => e.key === modelData.key))
                        background: null
                        contentItem: Item {
                            Item {
                                id: epThumbWrap
                                width: ep.width
                                height: width * 9 / 16
                                y: ep.pressed ? -1 : ep.hovered ? -5 : 0
                                scale: ep.pressed ? 0.97 : ep.hovered ? 1.038 : 1.0
                                transformOrigin: Item.Center
                                Behavior on y { NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
                                Behavior on scale { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring } }

                                Shadow {
                                    target: epCover
                                    lift: ep.pressed ? 0.3 : ep.hovered ? 1.1 : 0
                                    visible: epCover.ready
                                }
                                Cover {
                                    id: epCover
                                    anchors.fill: parent
                                    source: ep.modelData.image || page.media.banner || page.media.cover || ""
                                    title: ep.modelData.title
                                    zoom: 1
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.radiusMd
                                    color: "black"
                                    opacity: ep.hovered ? 0.28 : (ep.state && ep.state.completed ? 0.45 : 0)
                                    Behavior on opacity { NumberAnimation { duration: Motion.normal } }
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.radiusMd
                                    color: "transparent"
                                    border.width: 1
                                    border.color: ep.hovered ? Theme.alpha("#ffffff", Theme.dark ? 0.28 : 0.5) : Theme.alpha(Theme.text, 0.08)
                                    Behavior on border.color { ColorAnimation { duration: Motion.normal } }
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
                                // Download: start it, see it go, or delete it.
                                IconButton {
                                    readonly property var job: page.downloadStates[ep.modelData.key] || null
                                    visible: page.downloadSource !== null || job !== null
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 4
                                    diameter: 32
                                    iconSize: 16
                                    iconName: !job ? "download" : job.status === "done" ? "check" : job.status === "failed" ? "triangle-alert" : job.status === "running" ? "loader-circle" : "clock"
                                    iconColor: "white"
                                    background: Rectangle { radius: 16; color: Theme.alpha("#000000", 0.6) }
                                    tip: !job ? qsTr("Download")
                                        : job.status === "done" ? qsTr("Downloaded: delete")
                                        : job.status === "failed" ? qsTr("Download failed: try again")
                                        : job.status === "running" && job.total > 0 ? Math.floor(job.done / job.total * 100) + "%" + qsTr(": stop")
                                        : qsTr("Waiting: stop")
                                    onClicked: {
                                        if (!job) page.download([ep.modelData])
                                        else if (job.status === "failed") page.download([ep.modelData])
                                        else Downloads.removeUnit(page.seriesId, ep.modelData.key)
                                    }
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
                            Column {
                                y: epThumbWrap.height + 10 + (ep.hovered ? 2 : 0)
                                Behavior on y { NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
                                width: ep.width
                                spacing: 3
                                Text {
                                    width: parent.width
                                    text: ep.modelData.title
                                    color: Theme.text
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textSm
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: ep.modelData.airDate ? App.relativeTime(ep.modelData.airDate)
                                        : ep.modelData.released ? App.relativeTime(ep.modelData.released) : ep.modelData.filler ? qsTr("Filler") : ""
                                    color: Theme.textFaint
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textXs
                                }
                            }
                        }
                    }
                }
            }

            StateView {
                width: parent.width
                height: visible ? 220 : 0
                task: page.malTask && !page.malTask.ok ? page.malTask : page.episodesTask
                empty: page.episodesTask && page.episodesTask.ok && page.episodes.length === 0 && page.seasons.length === 0
                emptyTitle: qsTr("No episodes yet")
                emptyText: page.fromSource ? qsTr("The source lists no episodes for it.")
                    : page.media.nextEpisode ? qsTr("Episode ") + page.media.nextEpisode + qsTr(" airs ") + App.relativeTime(page.media.nextAiringAt) + "." : qsTr("This anime has not aired yet.")
                emptyIcon: "calendar"
                onRetry: page.episodesTask = page.fromSource ? Anime.sourceAnime(page.sourceUrl, page.summary) : Anime.episodes(page.anilistId)
            }

            // A source that splits the anime: each season opens on its own.
            Shelf {
                title: qsTr("Seasons")
                items: page.seasons
                subtitleFor: (it) => it.season >= 0 ? qsTr("Season %1").arg(it.season) : ""
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }

            // Its comic and novel, and where it leaves off in them.
            StoryPanel {
                visible: !Router.animeOnly
                width: parent.width
                seriesId: page.seriesId
                media: "anime"
                title: page.media.title || ""
                anilistId: page.fromSource ? 0 : page.anilistId
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

    WatchFromDialog { id: watchFrom }

    TrackingDialog {
        id: tracking
        seriesId: page.seriesId
        media: "anime"
        seriesTitle: page.media.title || ""
    }
}
