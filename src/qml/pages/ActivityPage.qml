import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Time spent in SCANCORE and chapters and episodes finished, on every synced
// device: today, this week, this month and all time, the streak, a year of
// days as a heat map, and a graph. Comics, novels and anime share one heat
// map: each day is coloured by what filled it.
Item {
    id: page
    property var task: Activity.summary()
    property var stats: null
    Connections {
        target: page.task
        function onFinished(result) { page.stats = result }
    }
    Connections {
        target: Activity
        function onChanged() { refresh.restart() }
    }
    Timer { id: refresh; interval: 1500; onTriggered: page.task = Activity.summary() }

    // "time" or "units"; and "", "comic", "novel" or "anime".
    property string metric: App.settings.activityMetric === "units" ? "units" : "time"
    property string media: ""
    // "days", "weeks" or "months".
    property string span: "days"

    readonly property var mediaKeys: ["comic", "novel", "anime"]
    readonly property var mediaLabels: ({ comic: qsTr("Comics"), novel: qsTr("Novels"), anime: qsTr("Anime"), app: qsTr("Browsing") })
    function mediaColor(key) {
        const name = { comic: "sky", novel: "amber", anime: "rose" }[key]
        if (!name) return Theme.textFaint
        // A colour, not the "#rrggbb" text, so blend() can read its channels.
        return Qt.color(Theme.dark ? Theme.accents[name].dark : Theme.accents[name].light)
    }

    function duration(seconds) {
        seconds = Math.max(0, Math.round(seconds || 0))
        const h = Math.floor(seconds / 3600)
        const m = Math.floor((seconds % 3600) / 60)
        if (h >= 100) return h + " h"
        if (h > 0) return h + " h " + m + " m"
        return m + " m"
    }

    function unitsText(t) {
        if (!t) return ""
        const chapters = (t.comic || 0) + (t.novel || 0)
        const parts = []
        parts.push(chapters + (chapters === 1 ? qsTr(" chapter") : qsTr(" chapters")))
        parts.push((t.anime || 0) + ((t.anime || 0) === 1 ? qsTr(" episode") : qsTr(" episodes")))
        return parts.join(" · ")
    }

    // One day's value for the heat map and graph.
    function valueOf(day, key) {
        if (!day) return 0
        const keys = key ? [key] : mediaKeys
        let v = 0
        for (const k of keys)
            v += metric === "time" ? day[k].seconds : day[k].units
        // Browsing time counts toward "all" when measuring time.
        if (!key && metric === "time") v += day.app.seconds
        return v
    }

    // A day's colour: each kind of media in proportion to its share.
    function blend(day) {
        if (media) return mediaColor(media)
        let r = 0, g = 0, b = 0, total = 0
        for (const k of mediaKeys) {
            const w = metric === "time" ? day[k].seconds : day[k].units
            if (w <= 0) continue
            const c = mediaColor(k)
            r += c.r * w; g += c.g * w; b += c.b * w; total += w
        }
        if (total === 0) return Theme.textMuted
        return Qt.rgba(r / total, g / total, b / total, 1)
    }

    function isoDay(d) {
        return d.getFullYear() + "-" + String(d.getMonth() + 1).padStart(2, "0") + "-" + String(d.getDate()).padStart(2, "0")
    }
    function dateOf(key) {
        const p = key.split("-")
        return new Date(Number(p[0]), Number(p[1]) - 1, Number(p[2]))
    }

    // The heat map: 53 weeks of days, Monday first, up to today.
    readonly property var cells: {
        if (!stats) return []
        const out = []
        const start = dateOf(stats.first)
        for (let i = 0; i < 53 * 7; ++i) {
            const d = new Date(start.getFullYear(), start.getMonth(), start.getDate() + i)
            const key = isoDay(d)
            out.push({ key: key, date: d, day: stats.days[key] || null, future: key > stats.todayKey })
        }
        return out
    }
    // Four steps of colour, split at the quarters of the days with anything.
    readonly property var steps: {
        const values = cells.map(c => valueOf(c.day, media)).filter(v => v > 0).sort((a, b) => a - b)
        if (values.length === 0) return [1, 2, 3]
        const q = (f) => values[Math.min(values.length - 1, Math.floor(values.length * f))]
        return [q(0.25), q(0.5), q(0.75)]
    }
    function level(v) {
        if (v <= 0) return 0
        return v <= steps[0] ? 1 : v <= steps[1] ? 2 : v <= steps[2] ? 3 : 4
    }

    // The graph: the last 30 days, 12 weeks or 12 months, each split by media.
    readonly property var bars: {
        if (!stats) return []
        const today = dateOf(stats.todayKey)
        const out = []
        const add = (label, keys) => {
            const parts = {}
            for (const k of mediaKeys.concat(["app"])) parts[k] = 0
            for (const key of keys) {
                const day = stats.days[key]
                if (!day) continue
                for (const k of mediaKeys) parts[k] += metric === "time" ? day[k].seconds : day[k].units
                if (metric === "time") parts.app += day.app.seconds
            }
            if (media) for (const k of mediaKeys.concat(["app"])) if (k !== media) parts[k] = 0
            out.push({ label: label, parts: parts, total: Object.values(parts).reduce((a, b) => a + b, 0), keys: keys })
        }
        if (span === "days") {
            for (let i = 29; i >= 0; --i) {
                const d = new Date(today.getFullYear(), today.getMonth(), today.getDate() - i)
                add(i % 5 === 0 ? Qt.formatDate(d, "d MMM") : "", [isoDay(d)])
            }
        } else if (span === "weeks") {
            const monday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - ((today.getDay() + 6) % 7))
            for (let w = 11; w >= 0; --w) {
                const keys = []
                for (let i = 0; i < 7; ++i)
                    keys.push(isoDay(new Date(monday.getFullYear(), monday.getMonth(), monday.getDate() - w * 7 + i)))
                add(w % 3 === 0 ? Qt.formatDate(dateOf(keys[0]), "d MMM") : "", keys)
            }
        } else {
            for (let m = 11; m >= 0; --m) {
                const first = new Date(today.getFullYear(), today.getMonth() - m, 1)
                const keys = []
                for (let d = new Date(first); d.getMonth() === first.getMonth(); d.setDate(d.getDate() + 1))
                    keys.push(isoDay(d))
                add(Qt.formatDate(first, "MMM"), keys)
            }
        }
        return out
    }
    readonly property real barMax: Math.max(1, ...bars.map(b => b.total))

    function formatValue(v) {
        return metric === "time" ? duration(v) : String(v)
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: body.height + 48

        Column {
            id: body
            width: flick.width
            spacing: 22

            PageHeader {
                title: qsTr("Activity")
                subtitle: qsTr("Your reading and watching, on every device you sync")
                ScButton {
                    text: qsTr("Your ") + new Date().getFullYear()
                    iconName: "gift"
                    onClicked: Router.push("pages/YearReview.qml", { year: new Date().getFullYear() })
                }
            }

            StateView {
                width: parent.width
                height: visible ? 300 : 0
                visible: page.stats === null
                task: page.task
                onRetry: page.task = Activity.summary()
            }

            // Today, this week, this month, all time, and the streak.
            Flow {
                visible: page.stats !== null
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 12
                readonly property int columns: width > 1100 ? 5 : width > 640 ? 3 : 2
                readonly property real tileWidth: (width - spacing * (columns - 1)) / columns

                Repeater {
                    model: [
                        { key: "today", label: qsTr("Today") },
                        { key: "week", label: qsTr("This week") },
                        { key: "month", label: qsTr("This month") },
                        { key: "all", label: qsTr("All time") }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        readonly property var t: page.stats ? page.stats[modelData.key] : null
                        width: parent.tileWidth
                        height: 128
                        radius: Theme.radiusLg
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        Column {
                            x: 16
                            y: 14
                            width: parent.width - 32
                            spacing: 4
                            Text {
                                text: modelData.label
                                color: Theme.textFaint
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textXs
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 1.1
                            }
                            Text {
                                text: t ? page.duration(t.seconds) : ""
                                color: Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: Theme.text2xl
                                font.weight: Font.Bold
                            }
                            Text {
                                width: parent.width
                                text: page.unitsText(t)
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                                elide: Text.ElideRight
                            }
                        }
                        // Where the time went.
                        Row {
                            x: 16
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 16
                            width: parent.width - 32
                            height: 5
                            Repeater {
                                model: page.mediaKeys.concat(["app"])
                                Rectangle {
                                    required property string modelData
                                    readonly property real share: t && t.seconds > 0 ? t.byMedia[modelData] / t.seconds : 0
                                    width: parent.width * share
                                    height: parent.height
                                    color: page.mediaColor(modelData)
                                    opacity: modelData === "app" ? 0.45 : 1
                                }
                            }
                            Rectangle {
                                visible: !t || t.seconds === 0
                                width: parent.width
                                height: parent.height
                                radius: 3
                                color: Theme.surface3
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.tileWidth
                    height: 128
                    radius: Theme.radiusLg
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border
                    Column {
                        x: 16
                        y: 14
                        width: parent.width - 32
                        spacing: 4
                        Text {
                            text: qsTr("Streak")
                            color: Theme.textFaint
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textXs
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 1.1
                        }
                        Row {
                            spacing: 8
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "flame"
                                size: 22
                                color: page.stats && page.stats.streak > 0 ? Theme.warning : Theme.textFaint
                            }
                            Text {
                                text: page.stats ? page.stats.streak + (page.stats.streak === 1 ? " day" : " days") : ""
                                color: Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: Theme.text2xl
                                font.weight: Font.Bold
                            }
                        }
                        Text {
                            width: parent.width
                            text: page.stats ? qsTr("Longest ") + page.stats.longestStreak + (page.stats.longestStreak === 1 ? " day" : " days") : ""
                            color: Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textSm
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // The year as a heat map.
            Rectangle {
                visible: page.stats !== null
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                height: heat.height + 36
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Column {
                    id: heat
                    x: 18
                    y: 18
                    width: parent.width - 36
                    spacing: 16

                    Flow {
                        width: parent.width
                        spacing: 10
                        Text {
                            height: 34
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("The last year")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.textXl
                            font.weight: Font.DemiBold
                            rightPadding: 12
                        }
                        Segmented {
                            options: [{ value: "time", label: qsTr("Time") }, { value: "units", label: qsTr("Chapters & episodes") }]
                            value: page.metric
                            onPicked: (v) => App.setSetting("activityMetric", v)
                        }
                        // Phones: the card's width, without icons, so it stays inside.
                        Segmented {
                            fill: Theme.compact
                            width: fill ? parent.width : implicitWidth
                            options: [{ value: "", label: qsTr("All") }, { value: "comic", label: qsTr("Comics"), icon: Theme.compact ? "" : "book-open" },
                                      { value: "novel", label: qsTr("Novels"), icon: Theme.compact ? "" : "book-text" },
                                      { value: "anime", label: qsTr("Anime"), icon: Theme.compact ? "" : "tv" }]
                            value: page.media
                            onPicked: (v) => page.media = v
                        }
                    }

                    Flickable {
                        id: heatScroll
                        width: parent.width
                        height: grid.height
                        contentWidth: grid.width
                        clip: true
                        interactive: contentWidth > width
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.HorizontalFlick
                        // The most recent weeks are on the right.
                        onContentWidthChanged: contentX = Math.max(0, contentWidth - width)
                        onWidthChanged: contentX = Math.max(0, contentWidth - width)

                        Item {
                            id: grid
                            readonly property int labelWidth: 30
                            readonly property real cell: Math.max(10, Math.min(15, (heatScroll.width - labelWidth) / 53 - 3))
                            readonly property real pitch: cell + 3
                            width: labelWidth + 53 * pitch
                            height: 18 + 7 * pitch

                            // Month names above the first week of each month.
                            Repeater {
                                model: 53
                                Text {
                                    required property int index
                                    readonly property var first: page.cells[index * 7]
                                    readonly property var previous: index > 0 ? page.cells[(index - 1) * 7] : null
                                    visible: !!first && (index === 0 || (!!previous && first.date.getMonth() !== previous.date.getMonth()))
                                    x: grid.labelWidth + index * grid.pitch
                                    text: first ? Qt.formatDate(first.date, "MMM") : ""
                                    color: Theme.textFaint
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textXs
                                }
                            }
                            Repeater {
                                model: [[qsTr("Mon"), 0], [qsTr("Wed"), 2], [qsTr("Fri"), 4]]
                                Text {
                                    required property var modelData
                                    y: 18 + modelData[1] * grid.pitch + (grid.cell - height) / 2
                                    text: modelData[0]
                                    color: Theme.textFaint
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textXs
                                }
                            }
                            Repeater {
                                model: page.cells
                                Rectangle {
                                    required property var modelData
                                    required property int index
                                    readonly property int lvl: page.level(page.valueOf(modelData.day, page.media))
                                    visible: !modelData.future
                                    x: grid.labelWidth + Math.floor(index / 7) * grid.pitch
                                    y: 18 + (index % 7) * grid.pitch
                                    width: grid.cell
                                    height: grid.cell
                                    radius: grid.cell / 2.6
                                    color: lvl === 0 ? Theme.surface3 : Theme.alpha(page.blend(modelData.day), [0, 0.32, 0.55, 0.78, 1][lvl])
                                    border.width: modelData.key === page.stats.todayKey ? 1 : 0
                                    border.color: Theme.textMuted
                                }
                            }
                            MouseArea {
                                id: heatHover
                                x: grid.labelWidth
                                y: 18
                                width: 53 * grid.pitch
                                height: 7 * grid.pitch
                                hoverEnabled: true
                                readonly property int index: containsMouse
                                    ? Math.floor(mouseX / grid.pitch) * 7 + Math.floor(mouseY / grid.pitch) : -1
                                readonly property var cell: index >= 0 && index < page.cells.length && !page.cells[index].future ? page.cells[index] : null
                            }
                        }
                    }

                    // The day under the pointer, or the legend.
                    Item {
                        width: parent.width
                        height: Math.max(22, legend.visible ? legend.height : 0)
                        Text {
                            visible: heatHover.cell !== null
                            anchors.verticalCenter: parent.verticalCenter
                            text: {
                                const c = heatHover.cell
                                if (!c) return ""
                                const parts = []
                                for (const k of page.mediaKeys) {
                                    const d = c.day ? c.day[k] : null
                                    if (d && (d.seconds > 0 || d.units > 0))
                                        parts.push(page.mediaLabels[k] + " " + page.duration(d.seconds)
                                                   + (d.units > 0 ? " (" + d.units + ")" : ""))
                                }
                                if (c.day && c.day.app.seconds > 0) parts.push(qsTr("Browsing ") + page.duration(c.day.app.seconds))
                                return Qt.formatDate(c.date, "dddd d MMMM yyyy") + "  ·  " + (parts.length ? parts.join("  ·  ") : qsTr("Nothing"))
                            }
                            color: Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textSm
                            width: parent.width
                            elide: Text.ElideRight
                        }
                        // Wraps on phones rather than running past the card.
                        Flow {
                            id: legend
                            visible: heatHover.cell === null
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            spacing: 18
                            Row {
                                visible: page.media === ""
                                spacing: 14
                                Repeater {
                                    model: page.mediaKeys
                                    Row {
                                        required property string modelData
                                        spacing: 6
                                        Rectangle { width: 10; height: 10; radius: 4; color: page.mediaColor(modelData); anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: page.mediaLabels[modelData]; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs }
                                    }
                                }
                            }
                            Row {
                                spacing: 4
                                Text { text: "Less"; color: Theme.textFaint; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; rightPadding: 4 }
                                Repeater {
                                    model: 5
                                    Rectangle {
                                        required property int index
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 10
                                        height: 10
                                        radius: 4
                                        color: index === 0 ? Theme.surface3
                                            : Theme.alpha(page.media ? page.mediaColor(page.media) : Theme.textMuted, [0, 0.32, 0.55, 0.78, 1][index])
                                    }
                                }
                                Text { text: "More"; color: Theme.textFaint; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; leftPadding: 4 }
                            }
                        }
                    }
                }
            }

            // Days, weeks or months as stacked bars.
            Rectangle {
                visible: page.stats !== null
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                height: graphColumn.height + 36
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Column {
                    id: graphColumn
                    x: 18
                    y: 18
                    width: parent.width - 36
                    spacing: 16
                    Flow {
                        width: parent.width
                        spacing: 10
                        Text {
                            height: 34
                            verticalAlignment: Text.AlignVCenter
                            text: page.metric === "time" ? qsTr("Time spent") : qsTr("Chapters and episodes")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.textXl
                            font.weight: Font.DemiBold
                            rightPadding: 12
                        }
                        Segmented {
                            options: [{ value: "days", label: qsTr("30 days") }, { value: "weeks", label: qsTr("12 weeks") }, { value: "months", label: qsTr("12 months") }]
                            value: page.span
                            onPicked: (v) => page.span = v
                        }
                    }
                    Item {
                        id: chart
                        width: parent.width
                        height: 210
                        readonly property int axisWidth: 56
                        readonly property real plotHeight: height - 22

                        // Gridlines at the top, middle and bottom.
                        Repeater {
                            model: [1, 0.5, 0]
                            Item {
                                required property real modelData
                                width: chart.width
                                y: chart.plotHeight * (1 - modelData)
                                Rectangle { x: chart.axisWidth; width: parent.width - chart.axisWidth; height: 1; color: Theme.border }
                                Text {
                                    y: -height / 2
                                    width: chart.axisWidth - 8
                                    horizontalAlignment: Text.AlignRight
                                    text: modelData === 0 ? "0" : page.formatValue(page.metric === "time" ? page.barMax * modelData : Math.round(page.barMax * modelData))
                                    color: Theme.textFaint
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textXs
                                }
                            }
                        }
                        Row {
                            id: barRow
                            x: chart.axisWidth + 4
                            width: chart.width - chart.axisWidth - 4
                            height: chart.height
                            readonly property real slot: width / Math.max(1, page.bars.length)
                            Repeater {
                                model: page.bars
                                Item {
                                    id: barSlot
                                    required property var modelData
                                    required property int index
                                    width: barRow.slot
                                    height: barRow.height
                                    Column {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 22
                                        width: Math.max(3, barRow.slot * 0.62)
                                        // Stacked from the bottom: comics, novels, anime, browsing.
                                        Repeater {
                                            model: ["app", "anime", "novel", "comic"]
                                            Rectangle {
                                                required property string modelData
                                                width: parent.width
                                                height: chart.plotHeight * (barSlot.modelData.parts[modelData] || 0) / page.barMax
                                                color: page.mediaColor(modelData)
                                                opacity: modelData === "app" ? 0.45 : barHover.containsMouse ? 1 : 0.9
                                            }
                                        }
                                    }
                                    Text {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: barSlot.modelData.label
                                        color: Theme.textFaint
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.textXs
                                    }
                                    MouseArea {
                                        id: barHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                    ToolTip.visible: barHover.containsMouse
                                    ToolTip.delay: 200
                                    ToolTip.text: {
                                        const keys = barSlot.modelData.keys
                                        const head = keys.length === 1 ? Qt.formatDate(page.dateOf(keys[0]), "d MMM")
                                            : Qt.formatDate(page.dateOf(keys[0]), "d MMM") + " – " + Qt.formatDate(page.dateOf(keys[keys.length - 1]), "d MMM")
                                        const parts = []
                                        for (const k of page.mediaKeys.concat(["app"]))
                                            if (barSlot.modelData.parts[k] > 0) parts.push(page.mediaLabels[k] + ": " + page.formatValue(barSlot.modelData.parts[k]))
                                        return head + "\n" + (parts.length ? parts.join("\n") : qsTr("Nothing"))
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // All time, by media.
            Rectangle {
                visible: page.stats !== null
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                height: breakdown.height + 36
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                Column {
                    id: breakdown
                    x: 18
                    y: 18
                    width: parent.width - 36
                    spacing: 14
                    Text {
                        text: qsTr("All time")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textXl
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: page.mediaKeys.concat(["app"])
                        Column {
                            required property string modelData
                            readonly property var all: page.stats ? page.stats.all : null
                            readonly property real seconds: all ? all.byMedia[modelData] : 0
                            width: breakdown.width
                            spacing: 6
                            Item {
                                width: parent.width
                                height: 20
                                Row {
                                    spacing: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    Rectangle { width: 10; height: 10; radius: 4; color: page.mediaColor(modelData); anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: page.mediaLabels[modelData]; color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.textMd; font.weight: Font.DemiBold }
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: page.duration(seconds) + (modelData === "app" || !all ? ""
                                        : "  ·  " + all[modelData] + (modelData === "anime" ? (all[modelData] === 1 ? qsTr(" episode") : qsTr(" episodes"))
                                                                                            : (all[modelData] === 1 ? qsTr(" chapter") : qsTr(" chapters"))))
                                    color: Theme.textMuted
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textSm
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: 6
                                radius: 3
                                color: Theme.surface3
                                Rectangle {
                                    width: parent.width * (all && all.seconds > 0 ? seconds / all.seconds : 0)
                                    height: parent.height
                                    radius: 3
                                    color: page.mediaColor(modelData)
                                    Behavior on width { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                                }
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: page.stats && page.stats.since
                            ? qsTr("Time counts while SCANCORE is in use: in a reader, playing an episode, or browsing. Chapters finished before time was kept come from your reading history.")
                            : ""
                        color: Theme.textFaint
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textXs
                    }
                }
            }
        }
    }

    BackToTop { flick: flick }
}
