import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import ScanCore

// SCANCORE Wrapped: the year as a story, one slide at a time across the whole
// window. Tap the right of the screen (or press →) for the next slide and the
// left for the previous one; holding pauses it. Slides with nothing to show
// are left out, and the last one is a card that can be saved as a picture.
// Only the slide on screen exists, and nothing is drawn with effects.
Item {
    id: page
    property int year: new Date().getFullYear()
    readonly property bool edgeToEdge: true
    readonly property real insetTop: SafeArea.margins.top
    readonly property real insetBottom: SafeArea.margins.bottom
    focus: true

    property var task: Activity.year(year)
    property var stats: null
    Connections {
        target: page.task
        function onFinished(result) { page.stats = result }
    }

    readonly property var monthNames: [qsTr("January"), qsTr("February"), qsTr("March"), qsTr("April"), qsTr("May", "month"), qsTr("June"), qsTr("July"), qsTr("August"),
                                       qsTr("September"), qsTr("October"), qsTr("November"), qsTr("December")]
    readonly property var dayNames: [qsTr("Mondays"), qsTr("Tuesdays"), qsTr("Wednesdays"), qsTr("Thursdays"), qsTr("Fridays"), qsTr("Saturdays"), qsTr("Sundays")]

    function number(n) { return Number(n || 0).toLocaleString(Qt.locale(), "f", 0) }
    function hours(seconds) { return Math.floor((seconds || 0) / 3600) }
    function timeText(seconds) {
        const h = hours(seconds)
        if (h >= 1) return number(h) + (h === 1 ? qsTr(" hour") : qsTr(" hours"))
        const m = Math.round((seconds || 0) / 60)
        return number(m) + (m === 1 ? qsTr(" minute") : qsTr(" minutes"))
    }
    function unitsText(units, media) {
        return number(units) + (media === "anime" ? (units === 1 ? qsTr(" episode") : qsTr(" episodes"))
                                                  : (units === 1 ? qsTr(" chapter") : qsTr(" chapters")))
    }
    function indexOfMax(list, value) {
        let best = -1, bestValue = 0
        list.forEach((x, i) => { const v = value(x); if (v > bestValue) { bestValue = v; best = i } })
        return best
    }
    // A day and month in the app's language (Qt.formatDate follows the system's).
    function dayMonth(d) { return qsTr("%1 %2", "day, month").replace("%1", d.getDate()).replace("%2", monthNames[d.getMonth()]) }
    function dateText(iso) { return iso ? dayMonth(new Date(iso + "T12:00:00")) : "" }

    readonly property bool empty: stats !== null && stats.total.seconds === 0 && stats.total.units === 0
    readonly property int busiestMonth: stats ? indexOfMax(stats.months, m => m.seconds + m.units * 600) : -1
    readonly property int favouriteDay: stats ? indexOfMax(stats.weekdays, s => s) : -1
    readonly property var mediaUsed: stats ? ["comic", "novel", "anime"].filter(m => stats.topByMedia[m].length > 0) : []

    // The slides this year has something for, in order.
    readonly property var slides: {
        if (!stats || empty) return []
        const out = ["intro", "time", "counts"]
        if (stats.top.length > 0) out.push("top")
        if (mediaUsed.length > 1) out.push("media")
        if (stats.genres.length > 0) out.push("genres")
        if (stats.author && stats.author.name) out.push("author")
        if (busiestMonth >= 0) out.push("rhythm")
        if (stats.activeDays > 0) out.push("streak")
        if (stats.first && stats.first.title) out.push("first")
        out.push("card")
        return out
    }
    property int current: 0
    readonly property string slide: slides[current] || ""
    readonly property bool last: current >= slides.length - 1
    property bool held: false

    // Background colours, one pair per slide, whatever the app's theme.
    readonly property var colours: ({
        intro: ["#3b1d8f", "#0e0a24"], time: ["#b4361f", "#2a0b07"], counts: ["#0f6e66", "#04201e"],
        top: ["#7a1f5c", "#1c0716"], media: ["#1f4fa8", "#081428"], genres: ["#9a6a08", "#241802"],
        author: ["#5a2d91", "#140a22"], rhythm: ["#126b3a", "#04190d"], streak: ["#b5301f", "#220805"],
        first: ["#264a7a", "#070f1b"], card: ["#2b2b33", "#0b0b0e"]
    })
    readonly property var pair: colours[slide] || ["#202028", "#0b0b0e"]

    function next() {
        if (!last) current += 1
    }
    function previous() {
        if (current > 0) current -= 1
        else clock.restart()
    }
    onCurrentChanged: last ? clock.stop() : clock.restart()

    // How far through the slide on screen; it moves on by itself.
    property real progress: 0
    NumberAnimation {
        id: clock
        target: page
        property: "progress"
        from: 0
        to: 1
        duration: page.slide === "intro" ? 4500 : 7000
        running: page.slides.length > 0 && !page.last
        paused: running && (page.held || Qt.application.state !== Qt.ApplicationActive)
        onFinished: page.next()
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Right || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown) { page.next(); event.accepted = true }
        else if (event.key === Qt.Key_Left || event.key === Qt.Key_PageUp) { page.previous(); event.accepted = true }
        else if (event.key === Qt.Key_Escape) { Router.back(); event.accepted = true }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: page.pair[0]; Behavior on color { ColorAnimation { duration: Motion.slow } } }
            GradientStop { position: 1; color: page.pair[1]; Behavior on color { ColorAnimation { duration: Motion.slow } } }
        }
    }

    // Taps: the left third goes back, the rest forward; holding pauses.
    TapHandler {
        enabled: page.slides.length > 0
        onPressedChanged: page.held = pressed
        onTapped: (point) => {
            if (point.position.x < page.width / 3) page.previous()
            else page.next()
        }
    }

    // The bars along the top, one per slide, and the close button.
    Item {
        id: top
        x: 16
        y: page.insetTop + 12
        width: page.width - 32
        height: 40
        z: 2
        Row {
            id: bars
            width: parent.width - close.width - 12
            spacing: 4
            Repeater {
                model: page.slides.length
                Rectangle {
                    required property int index
                    width: (bars.width - (page.slides.length - 1) * 4) / Math.max(1, page.slides.length)
                    height: 3
                    radius: 1.5
                    color: Qt.rgba(1, 1, 1, 0.28)
                    Rectangle {
                        height: parent.height
                        radius: parent.radius
                        color: "white"
                        width: parent.width * (index < page.current ? 1 : index > page.current ? 0 : page.last ? 1 : page.progress)
                    }
                }
            }
        }
        Text {
            anchors.top: bars.bottom
            anchors.topMargin: 10
            text: qsTr("SCANCORE Wrapped ") + page.year
            color: Qt.rgba(1, 1, 1, 0.7)
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.2
        }
        IconButton {
            id: close
            anchors.right: parent.right
            y: -8
            iconName: "x"
            iconColor: "white"
            tip: qsTr("Close")
            onClicked: Router.back()
        }
    }

    // Loading, or a year with nothing in it.
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        spacing: 12
        visible: page.slides.length === 0
        Spinner { anchors.horizontalCenter: parent.horizontalCenter; visible: page.stats === null }
        Text {
            width: parent.width
            visible: page.empty
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Nothing yet in ") + page.year
            color: "white"
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.text2xl
            font.weight: Font.Bold
        }
        Text {
            width: parent.width
            visible: page.empty
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Chapters you finish and time spent reading and watching show up here.")
            color: Qt.rgba(1, 1, 1, 0.75)
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
        }
    }

    // The slide on screen.
    Loader {
        id: stage
        anchors.fill: parent
        anchors.topMargin: page.insetTop + 64
        anchors.bottomMargin: page.insetBottom + 24
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        active: page.slide !== ""
        sourceComponent: ({
            intro: introSlide, time: timeSlide, counts: countsSlide, top: topSlide, media: mediaSlide, genres: genresSlide,
            author: authorSlide, rhythm: rhythmSlide, streak: streakSlide, first: firstSlide, card: cardSlide
        })[page.slide] || null
        onLoaded: entrance.restart()
        ParallelAnimation {
            id: entrance
            NumberAnimation { target: stage.item; property: "opacity"; from: 0; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
            NumberAnimation { target: stage.item; property: "y"; from: 24; to: 0; duration: Motion.page; easing.type: Easing.OutCubic }
        }
    }

    // Shared text styles for the slides.
    component Kicker: Text {
        width: parent ? parent.width : implicitWidth
        wrapMode: Text.Wrap
        color: Qt.rgba(1, 1, 1, 0.75)
        font.family: Theme.fontSans
        font.pixelSize: Theme.textLg
        font.weight: Font.Medium
    }
    component Big: Text {
        width: parent ? parent.width : implicitWidth
        wrapMode: Text.Wrap
        color: "white"
        font.family: Theme.fontDisplay
        font.pixelSize: Math.min(page.width / 6, 88)
        font.weight: Font.Bold
        font.letterSpacing: -1.5
        lineHeight: 0.95
    }
    component Line: Text {
        width: parent ? parent.width : implicitWidth
        wrapMode: Text.Wrap
        color: "white"
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXl
        font.weight: Font.DemiBold
    }
    // A column of the slide's content, centred in the window and no wider than a phone held sideways.
    component Body: Column {
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
        width: Math.min(parent ? parent.width : 0, 560)
        spacing: 14
    }

    Component {
        id: introSlide
        Item {
            Body {
                spacing: 8
                Image {
                    source: App.icon("scancore-mark", "#ffffff")
                    sourceSize: Qt.size(44, 44)
                    width: 44
                    height: 44
                }
                Item { width: 1; height: 12 }
                Kicker { text: qsTr("Your year in comics, novels and anime") }
                Big { text: String(page.year); font.pixelSize: Math.min(page.width / 3.2, 180) }
                Item { width: 1; height: 16 }
                Kicker { text: Theme.touch ? qsTr("Tap to begin") : qsTr("Click or press → to begin"); font.pixelSize: Theme.textMd; opacity: 0.8 }
            }
        }
    }

    Component {
        id: timeSlide
        Item {
            id: timeRoot
            readonly property var byMedia: page.stats.total.byMedia
            readonly property real maxSeconds: Math.max(1, byMedia.comic, byMedia.novel, byMedia.anime)
            Body {
                Kicker { text: qsTr("This year you spent") }
                Big { text: page.timeText(page.stats.total.mediaSeconds) }
                Kicker { text: qsTr("reading and watching") }
                Item { width: 1; height: 18 }
                Repeater {
                    model: [{ key: "comic", label: qsTr("Comics") }, { key: "novel", label: qsTr("Novels") }, { key: "anime", label: qsTr("Anime") }]
                    Column {
                        required property var modelData
                        width: parent.width
                        spacing: 6
                        visible: timeRoot.byMedia[modelData.key] >= 30
                        Item {
                            width: parent.width
                            height: label.height
                            Text {
                                id: label
                                text: modelData.label
                                color: "white"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                font.weight: Font.DemiBold
                            }
                            Text {
                                anchors.right: parent.right
                                text: page.timeText(timeRoot.byMedia[modelData.key])
                                color: Qt.rgba(1, 1, 1, 0.75)
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                            }
                        }
                        Rectangle {
                            width: parent.width
                            height: 10
                            radius: 5
                            color: Qt.rgba(1, 1, 1, 0.16)
                            Rectangle {
                                height: parent.height
                                radius: parent.radius
                                color: "white"
                                width: Math.max(parent.height, parent.width * timeRoot.byMedia[modelData.key] / timeRoot.maxSeconds)
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: countsSlide
        Item {
            readonly property var t: page.stats.total
            Body {
                spacing: 22
                Kicker { text: qsTr("You finished") }
                Repeater {
                    model: [
                        { value: t.comic, label: qsTr("comic chapters") },
                        { value: t.novel, label: qsTr("novel chapters") },
                        { value: t.anime, label: qsTr("anime episodes") }
                    ].filter(x => x.value > 0)
                    Column {
                        required property var modelData
                        width: parent.width
                        Big { text: page.number(modelData.value); font.pixelSize: Math.min(page.width / 7, 72) }
                        Line { text: modelData.label; color: Qt.rgba(1, 1, 1, 0.8) }
                    }
                }
                Kicker {
                    visible: page.stats.series > 0
                    text: page.stats.series === 1 ? qsTr("in one series") : qsTr("across %1 series").arg(page.number(page.stats.series))
                }
            }
        }
    }

    Component {
        id: topSlide
        Item {
            readonly property var first: page.stats.top[0]
            readonly property var rest: page.stats.top.slice(1, 5)
            readonly property bool wide: width > height * 1.1
            Body {
                Kicker { text: qsTr("The series you came back to most") }
                Flow {
                    width: parent.width
                    spacing: 24
                    Cover {
                        width: wide ? Math.min(220, parent.width * 0.35) : Math.min(parent.width * 0.55, 220, (page.height - 420) / 1.45)
                        height: width * 1.45
                        radius: Theme.radiusLg
                        source: first.cover || ""
                        title: first.title
                    }
                    Column {
                        width: wide ? parent.width - Math.min(220, parent.width * 0.35) - 24 : parent.width
                        spacing: 10
                        Line { text: first.title; font.pixelSize: Theme.text2xl; font.family: Theme.fontDisplay; font.weight: Font.Bold; maximumLineCount: 3; elide: Text.ElideRight }
                        Kicker { text: page.unitsText(first.units, first.media) }
                        Item { width: 1; height: 8; visible: rest.length > 0 }
                        Repeater {
                            model: rest
                            Text {
                                required property var modelData
                                required property int index
                                width: parent.width
                                elide: Text.ElideRight
                                text: (index + 2) + "   " + modelData.title
                                color: Qt.rgba(1, 1, 1, 0.85)
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textLg
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: mediaSlide
        Item {
            id: mediaRoot
            readonly property var labels: ({ comic: qsTr("Comic"), novel: qsTr("Novel"), anime: qsTr("Anime") })
            Body {
                Kicker { text: qsTr("Your number one of each") }
                Row {
                    id: tops
                    width: parent.width
                    spacing: 16
                    readonly property real column: (width - (page.mediaUsed.length - 1) * spacing) / page.mediaUsed.length
                    Repeater {
                        model: page.mediaUsed
                        Column {
                            required property string modelData
                            readonly property var item: page.stats.topByMedia[modelData][0]
                            width: tops.column
                            spacing: 8
                            Text {
                                text: mediaRoot.labels[modelData]
                                color: Qt.rgba(1, 1, 1, 0.7)
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textXs
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 1.1
                            }
                            Cover {
                                width: Math.min(parent.width, 180, (page.height - 360) / 1.45)
                                height: width * 1.45
                                radius: Theme.radiusMd
                                source: item.cover || ""
                                title: item.title
                            }
                            Text {
                                width: parent.width
                                text: item.title
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                color: "white"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: page.unitsText(item.units, item.media)
                                color: Qt.rgba(1, 1, 1, 0.7)
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: genresSlide
        Item {
            id: genresRoot
            readonly property real maxUnits: Math.max(1, page.stats.genres[0].units)
            Body {
                spacing: 16
                Kicker { text: qsTr("The genres you lived in") }
                Repeater {
                    model: page.stats.genres
                    Item {
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: Math.max(name.height, 30)
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height + 12
                            radius: Theme.radiusMd
                            color: Qt.rgba(1, 1, 1, index === 0 ? 0.22 : 0.12)
                            width: Math.max(height, parent.width * modelData.units / genresRoot.maxUnits)
                        }
                        Text {
                            id: name
                            x: 14
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 28 - count.width
                            elide: Text.ElideRight
                            text: (index + 1) + ".  " + modelData.name
                            color: "white"
                            font.family: index === 0 ? Theme.fontDisplay : Theme.fontSans
                            font.pixelSize: index === 0 ? Theme.text2xl : Theme.textLg
                            font.weight: index === 0 ? Font.Bold : Font.DemiBold
                        }
                        Text {
                            id: count
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: page.number(modelData.units)
                            color: Qt.rgba(1, 1, 1, 0.75)
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textMd
                        }
                    }
                }
            }
        }
    }

    Component {
        id: authorSlide
        Item {
            Body {
                Kicker { text: qsTr("The author you read most") }
                Big { text: page.stats.author.name; font.pixelSize: Math.min(page.width / 9, 64) }
                Kicker { text: page.unitsText(page.stats.author.units, "comic") }
            }
        }
    }

    Component {
        id: rhythmSlide
        Item {
            id: rhythmRoot
            readonly property real maxMonth: Math.max(1, ...page.stats.months.map(m => m.seconds))
            Body {
                Kicker { text: qsTr("Your busiest month was") }
                Big { text: page.monthNames[page.busiestMonth] }
                Item {
                    id: chart
                    width: parent.width
                    height: Math.min(150, page.height * 0.2)
                    Row {
                        anchors.fill: parent
                        spacing: 5
                        Repeater {
                            model: page.stats.months
                            Item {
                                required property var modelData
                                required property int index
                                width: (chart.width - 11 * 5) / 12
                                height: chart.height
                                Rectangle {
                                    anchors.bottom: initial.top
                                    anchors.bottomMargin: 6
                                    width: parent.width
                                    height: Math.max(3, (parent.height - 24) * modelData.seconds / rhythmRoot.maxMonth)
                                    radius: 3
                                    color: index === page.busiestMonth ? "white" : Qt.rgba(1, 1, 1, 0.3)
                                }
                                Text {
                                    id: initial
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: page.monthNames[index].slice(0, 1)
                                    color: Qt.rgba(1, 1, 1, 0.6)
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textXs
                                }
                            }
                        }
                    }
                }
                Kicker {
                    visible: page.favouriteDay >= 0
                    text: page.favouriteDay >= 0 ? qsTr("and you read and watched most on ") + page.dayNames[page.favouriteDay] + "." : ""
                }
            }
        }
    }

    Component {
        id: streakSlide
        Item {
            Body {
                Kicker { text: qsTr("Your longest streak") }
                Big { text: page.stats.longestStreak + (page.stats.longestStreak === 1 ? qsTr(" day") : qsTr(" days")) }
                Kicker { text: qsTr("in a row, of %1 days with something read or watched").arg(page.number(page.stats.activeDays)) }
                Item { width: 1; height: 14; visible: page.stats.bestDay !== "" }
                Line {
                    visible: page.stats.bestDay !== "" && page.stats.bestDaySeconds > 0
                    text: qsTr("Your biggest day: ") + page.dateText(page.stats.bestDay) + ", " + page.timeText(page.stats.bestDaySeconds)
                }
            }
        }
    }

    Component {
        id: firstSlide
        Item {
            readonly property var first: page.stats.first
            Body {
                Kicker { text: qsTr("Where your year started") }
                Cover {
                    width: Math.min(parent.width * 0.45, 180, (page.height - 380) / 1.45)
                    height: width * 1.45
                    radius: Theme.radiusLg
                    source: first.cover || ""
                    title: first.title
                }
                Line { text: first.title; font.pixelSize: Theme.text2xl; font.family: Theme.fontDisplay; font.weight: Font.Bold; maximumLineCount: 3; elide: Text.ElideRight }
                Kicker { text: qsTr("the first series you finished something in, on ") + page.dayMonth(new Date(first.firstAt * 1000)) }
            }
        }
    }

    // The summary card, and saving it as a picture.
    Component {
        id: cardSlide
        Item {
            id: cardStage
            // The card is drawn at its own size and scaled to fit; the
            // picture is taken from the card itself, at three times that.
            readonly property real fit: Math.min(1, width / card.width, (height - actions.height - 20) / card.height)
            Item {
                id: holder
                anchors.horizontalCenter: parent.horizontalCenter
                width: card.width * cardStage.fit
                height: card.height * cardStage.fit
                Rectangle {
                    id: card
                    width: 360
                    height: cardContent.height + 100
                    scale: cardStage.fit
                    transformOrigin: Item.TopLeft
                    radius: 24
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#3b1d8f" }
                        GradientStop { position: 0.5; color: "#7a1f5c" }
                        GradientStop { position: 1; color: "#b4361f" }
                    }
                    Column {
                        id: cardContent
                        x: 28
                        y: 28
                        width: parent.width - 56
                        spacing: 18
                        Row {
                            spacing: 10
                            Image {
                                anchors.verticalCenter: parent.verticalCenter
                                source: App.icon("scancore-mark", "#ffffff")
                                sourceSize: Qt.size(24, 24)
                                width: 24
                                height: 24
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("My %1 in SCANCORE").arg(page.year)
                                color: "white"
                                font.family: Theme.fontDisplay
                                font.pixelSize: 20
                                font.weight: Font.Bold
                            }
                        }
                        Row {
                            spacing: 16
                            visible: page.stats.top.length > 0
                            Cover {
                                width: 96
                                height: 139
                                radius: 10
                                source: page.stats.top.length > 0 ? page.stats.top[0].cover || "" : ""
                                title: page.stats.top.length > 0 ? page.stats.top[0].title : ""
                            }
                            Column {
                                width: card.width - 56 - 96 - 16
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4
                                Text { text: qsTr("Top series"); color: Qt.rgba(1, 1, 1, 0.7); font.family: Theme.fontSans; font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                                Text {
                                    width: parent.width
                                    text: page.stats.top.length > 0 ? page.stats.top[0].title : ""
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                    color: "white"
                                    font.family: Theme.fontDisplay
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                }
                            }
                        }
                        Grid {
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 14
                            Repeater {
                                model: [
                                    { label: qsTr("Time"), value: page.timeText(page.stats.total.mediaSeconds) },
                                    { label: qsTr("Chapters"), value: page.number(page.stats.total.comic + page.stats.total.novel) },
                                    { label: qsTr("Episodes"), value: page.number(page.stats.total.anime) },
                                    { label: qsTr("Longest streak"), value: page.stats.longestStreak + (page.stats.longestStreak === 1 ? qsTr(" day") : qsTr(" days")) },
                                    page.stats.genres.length > 0 ? { label: qsTr("Top genre"), value: page.stats.genres[0].name }
                                                                  : { label: qsTr("Series"), value: page.number(page.stats.series) },
                                    { label: qsTr("Busiest month"), value: page.busiestMonth >= 0 ? page.monthNames[page.busiestMonth] : "—" }
                                ]
                                Column {
                                    required property var modelData
                                    width: (card.width - 56 - 16) / 2
                                    spacing: 2
                                    Text { text: modelData.label; color: Qt.rgba(1, 1, 1, 0.7); font.family: Theme.fontSans; font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                                    Text { width: parent.width; elide: Text.ElideRight; text: modelData.value; color: "white"; font.family: Theme.fontDisplay; font.pixelSize: 22; font.weight: Font.Bold }
                                }
                            }
                        }
                    }
                    Text {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 22
                        x: 28
                        text: "scancore.onl"
                        color: Qt.rgba(1, 1, 1, 0.7)
                        font.family: Theme.fontSans
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }
            Row {
                id: actions
                anchors.top: holder.bottom
                anchors.topMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                ScButton {
                    text: qsTr("Save picture")
                    iconName: "download"
                    variant: "primary"
                    onClicked: saveDialog.open()
                }
                ScButton {
                    text: qsTr("Watch again")
                    iconName: "rotate-ccw"
                    onClicked: page.current = 0
                }
            }
            FileDialog {
                id: saveDialog
                fileMode: FileDialog.SaveFile
                defaultSuffix: "png"
                nameFilters: [qsTr("PNG picture (*.png)")]
                selectedFile: "scancore-wrapped-" + page.year + ".png"
                onAccepted: {
                    const file = selectedFile
                    card.grabToImage((result) => {
                        const saved = Activity.saveImage(result.image, file)
                        Router.toast(saved ? qsTr("Picture saved") : qsTr("Could not save the picture"), saved ? "success" : "error")
                    }, Qt.size(card.width * 3, card.height * 3))
                }
            }
        }
    }
}
