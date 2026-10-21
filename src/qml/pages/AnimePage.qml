import QtQuick
import QtQuick.Effects
import QtQuick.Controls.Basic
import ScanCore

// Anime home: rotating spotlight, continue watching, today's schedule and
// AniList's trending, seasonal, popular and top lists.
Item {
    id: page
    property var homeTask: Anime.home()
    property var historyTask: Library.history("anime", 16)
    property var todayTask: Anime.schedule(Math.floor(new Date().setHours(0, 0, 0, 0) / 1000), Math.floor(new Date().setHours(23, 59, 59, 999) / 1000))
    readonly property var home: homeTask && homeTask.ok ? homeTask.result : null
    readonly property var spotlight: home ? home.trending.filter(m => m.banner).slice(0, 6) : []
    property int spot: 0

    function search(text) { Router.openAnimeBrowse({ q: text }) }

    Connections {
        target: Library
        function onChanged() { refresh.restart() }
    }
    Timer { id: refresh; interval: 500; onTriggered: page.historyTask = Library.history("anime", 16) }
    property var history: []
    Connections {
        target: page.historyTask
        function onFinished(result) { page.history = Router.keep(page.history, result) }
    }

    // Slide timing: the active dot fills as it runs; hovering the spotlight
    // pauses it.
    property real slideProgress: 0
    function showSlide(i) {
        const n = spotlight.length
        if (n === 0) return
        spot = (i + n) % n
        slideTimer.restart()
    }
    NumberAnimation {
        id: slideTimer
        target: page
        property: "slideProgress"
        from: 0
        to: 1
        duration: 7000
        running: page.spotlight.length > 1 && page.visible && !Motion.reduced
        paused: running && heroHover.hovered
        onFinished: page.showSlide(page.spot + 1)
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 36

            // ------------------------------------------------ spotlight
            Item {
                id: hero
                width: parent.width
                height: Math.min(520, Math.max(380, page.height * 0.56))
                clip: true

                HoverHandler { id: heroHover; enabled: !Theme.touch }
                // Swipe (or drag) sideways for the next or previous title.
                DragHandler {
                    target: null
                    yAxis.enabled: false
                    onActiveChanged: {
                        if (active || Math.abs(translation.x) < 60) return
                        page.showSlide(page.spot + (translation.x < 0 ? 1 : -1))
                    }
                }

                // Over a background picture or a see-through window there is no
                // page colour to fade into; the slides fade out themselves.
                Rectangle {
                    id: heroFade
                    anchors.fill: parent
                    visible: false
                    layer.enabled: true
                    gradient: Gradient {
                        GradientStop { position: 0.5; color: "white" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
                Item {
                    anchors.fill: parent
                    layer.enabled: Theme.layered
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: heroFade
                    }
                    Repeater {
                        model: page.spotlight
                        delegate: Item {
                            id: slide
                            required property var modelData
                            required property int index
                            readonly property bool current: index === page.spot
                            anchors.fill: parent
                            opacity: current ? 1 : 0
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
                            Image {
                                id: banner
                                anchors.fill: parent
                                source: App.image(slide.modelData.banner)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                sourceSize.width: 1920
                                scale: slide.current ? 1.08 : 1.0
                                Behavior on scale { NumberAnimation { duration: 8000; easing.type: Easing.Linear } }
                            }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.alpha(Theme.base, 0.95) }
                            GradientStop { position: 0.55; color: Theme.alpha(Theme.base, 0.45) }
                            GradientStop { position: 1.0; color: Theme.alpha(Theme.base, 0.1) }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.55; color: "transparent" }
                            GradientStop { position: 1.0; color: Theme.bg }
                        }
                    }
                }

                Column {
                    id: heroText
                    readonly property var media: page.spotlight[page.spot] || null
                    x: Theme.gutter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 48
                    width: Math.min(620, parent.width - 2 * Theme.gutter)
                    spacing: 14
                    visible: media !== null

                    // Re-run the entrance on each slide change.
                    onMediaChanged: textReveal.restart()
                    ParallelAnimation {
                        id: textReveal
                        NumberAnimation { target: heroText; property: "opacity"; from: 0; to: 1; duration: Motion.page; easing.type: Easing.OutCubic }
                        NumberAnimation { target: heroShift; property: "x"; from: -20; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
                    }
                    transform: Translate { id: heroShift }

                    Text {
                        text: "#" + (page.spot + 1) + qsTr(" trending")
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textXs
                        font.weight: Font.Bold
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 1.6
                    }
                    Text {
                        width: parent.width
                        text: heroText.media ? heroText.media.title : ""
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: 44
                        font.weight: Font.Bold
                        font.letterSpacing: -1
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        lineHeight: 1.0
                    }
                    Text {
                        text: heroText.media ? [heroText.media.format, heroText.media.year, heroText.media.episodes ? heroText.media.episodes + qsTr(" episodes") : "", heroText.media.score ? heroText.media.score + "%" : "", (heroText.media.genres || []).slice(0, 3).join(", ")].filter(Boolean).join("  ·  ") : ""
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textMd
                    }
                    Row {
                        spacing: 10
                        ScButton {
                            variant: "primary"
                            text: qsTr("Watch now")
                            iconName: "play"
                            onClicked: Router.openSeries(heroText.media, null)
                        }
                        ScButton {
                            text: heroText.media && Library.revision >= 0 && Library.isSaved(heroText.media.id) ? qsTr("In library") : qsTr("Add to library")
                            iconName: "bookmark"
                            onClicked: {
                                if (Library.isSaved(heroText.media.id)) Library.remove(heroText.media.id)
                                else Library.save(Anime.seriesFor(heroText.media))
                            }
                        }
                    }
                }

                // Phones: below the buttons, where nothing else sits.
                Row {
                    x: Theme.compact ? Theme.gutter : parent.width - width - Theme.gutter
                    y: parent.height - height - (Theme.compact ? 18 : 56)
                    spacing: 8
                    Repeater {
                        model: page.spotlight.length
                        Rectangle {
                            id: dot
                            required property int index
                            readonly property bool active: index === page.spot
                            width: active ? 28 : 8
                            height: 8
                            radius: 4
                            color: Theme.alpha(Theme.text, active ? 0.35 : 0.3)
                            clip: true
                            Behavior on width { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                            // Fills while the slide shows; solid when it does not advance.
                            Rectangle {
                                visible: dot.active
                                height: parent.height
                                radius: 4
                                width: slideTimer.running ? Math.max(8, parent.width * page.slideProgress) : parent.width
                                color: Theme.text
                            }
                            MouseArea { anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor; onClicked: page.showSlide(index) }
                        }
                    }
                }

                // Phones get a search button in the row below instead.
                SearchField {
                    visible: !Theme.compact
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.gutter
                    y: 24
                    width: 300
                    placeholderText: qsTr("Search anime")
                    onSubmitted: (text) => Router.openAnimeBrowse({ q: text })
                }
                Row {
                    x: Theme.gutter
                    y: 24
                    spacing: 8
                    ScButton { text: qsTr("Browse"); iconName: "compass"; compact: true; onClicked: Router.openAnimeBrowse({}) }
                    ScButton { text: qsTr("Schedule"); iconName: "calendar"; compact: true; onClicked: Router.openAnimeSchedule() }
                    ScButton { visible: Theme.compact; text: qsTr("Search"); iconName: "search"; compact: true; onClicked: Router.openAnimeBrowse({}) }
                }

                StateView {
                    anchors.fill: parent
                    task: page.homeTask
                    onRetry: page.homeTask = Anime.home()
                }
            }

            Shelf {
                title: qsTr("Continue watching")
                items: page.history
                subtitleFor: (it) => qsTr("Episode ") + (it.unitNumber || it.unitKey || "")
                progressFor: (it) => (it.completed && (it.percent >= 0.96 || (it.position && it.position.duration > 0 && it.position.seconds >= it.position.duration - 20))) ? 1 : (it.percent || 0)
                onOpened: (it, cover) => Router.resume(it)
            }
            Shelf {
                title: qsTr("Airing today")
                actionText: qsTr("Full schedule")
                onAction: Router.openAnimeSchedule()
                items: page.todayTask && page.todayTask.ok ? page.todayTask.result : []
                loading: page.todayTask ? page.todayTask.loading : false
                subtitleFor: (it) => qsTr("Ep ") + it.airingEpisode + " · " + Qt.formatTime(new Date(it.airingAt * 1000), "hh:mm")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
            Shelf {
                title: qsTr("Trending now")
                items: page.home ? page.home.trending : []
                loading: page.homeTask ? page.homeTask.loading : false
                subtitleFor: (it) => [it.format, it.year].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
            Shelf {
                title: page.home ? page.home.seasonName : qsTr("This season")
                items: page.home ? page.home.season : []
                loading: page.homeTask ? page.homeTask.loading : false
                subtitleFor: (it) => it.nextEpisode ? qsTr("Ep ") + it.nextEpisode + " " + App.relativeTime(it.nextAiringAt) : [it.format, it.status].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
            Shelf {
                title: qsTr("All-time popular")
                items: page.home ? page.home.popular : []
                subtitleFor: (it) => [it.format, it.year].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
            Shelf {
                title: qsTr("Top rated")
                items: page.home ? page.home.top : []
                badgeFor: (it) => it.score ? it.score + "%" : ""
                subtitleFor: (it) => [it.format, it.year].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
        }
    }

    BackToTop { flick: flick }
}
