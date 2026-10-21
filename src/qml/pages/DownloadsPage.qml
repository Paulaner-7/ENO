import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Offline chapters: queue, progress, failures and stored copies.
Item {
    id: page
    property var task: Downloads.items()
    property var sizeText: Downloads.sizeOnDisk()

    Connections {
        target: Downloads
        function onChanged() { refresh.restart() }
    }
    Timer {
        id: refresh
        interval: 250
        onTriggered: page.task = Downloads.items()
    }

    // Running downloads refresh this list several times a second. The rows
    // are replaced only when downloads come, go or change state; page counts
    // are read from `progress`, so rows (and the scroll position) stay put.
    property var items: []
    property var progress: ({})
    property string shape: ""
    Connections {
        target: page.task
        function onFinished(result) {
            const counts = {}
            for (const i of result) counts[i.id] = { done: i.done, total: i.total }
            page.progress = counts
            const shape = JSON.stringify(result.map(i => [i.id, i.status, i.error]))
            if (shape !== page.shape) {
                page.shape = shape
                page.items = result
            }
        }
    }
    // Measuring walks the whole downloads folder, so only when what is saved
    // changes, not on every page of a running download.
    readonly property int savedCount: items.filter(i => i.status === "done").length
    onSavedCountChanged: sizeText = Downloads.sizeOnDisk()
    readonly property int failedCount: items.filter(i => i.status === "failed").length

    ListView {
        WheelScroll {}
        id: list
        anchors.fill: parent
        model: page.items
        clip: true
        bottomMargin: 40
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScScrollBar {}

        header: Column {
            width: list.width
            spacing: 14
            PageHeader {
                title: qsTr("Downloads")
                subtitle: page.items.filter(i => i.status === "done").length + qsTr(" saved · ") + page.sizeText
                Row {
                    spacing: 8
                    ScButton { visible: page.failedCount > 0; text: qsTr("Retry failed"); iconName: "refresh-cw"; onClicked: Downloads.retryFailed() }
                    ScButton { visible: page.failedCount > 0; text: qsTr("Clear failed"); variant: "ghost"; onClicked: Downloads.clearFinished() }
                    ScButton { visible: App.platform !== "android"; text: qsTr("Open folder"); iconName: "folder-open"; onClicked: App.openFolder(Downloads.folder()) }
                }
            }
            // Android pauses downloads soon after the app leaves the screen
            // while battery optimisation is on.
            Rectangle {
                visible: App.platform === "android" && App.batteryRestricted
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                height: visible ? batteryRow.implicitHeight + 24 : 0
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                Row {
                    id: batteryRow
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 28
                    spacing: 12
                    Icon { anchors.verticalCenter: parent.verticalCenter; name: "battery-warning"; color: Theme.warning; size: 20 }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 32 - allowBattery.width - 24
                        text: qsTr("Battery optimisation pauses downloads when you leave SCANCORE.")
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                    ScButton { id: allowBattery; anchors.verticalCenter: parent.verticalCenter; compact: true; text: qsTr("Allow"); onClicked: App.askBatteryUnrestricted() }
                }
            }
            StateView {
                width: list.width
                height: visible ? 320 : 0
                task: page.items.length === 0 ? page.task : null
                empty: page.task && page.task.ok && page.items.length === 0
                emptyTitle: qsTr("No downloads yet")
                emptyText: qsTr("On a comic, novel or anime page, use the download button on a chapter or episode, or select several at once. Downloads open without a connection.")
                emptyIcon: "download"
            }
        }

        delegate: Item {
            id: row
            required property var modelData
            readonly property var count: page.progress[modelData.id] || { done: modelData.done, total: modelData.total }
            width: list.width
            height: 74

            Rectangle {
                x: Theme.gutter - 12
                width: parent.width - 2 * Theme.gutter + 24
                height: parent.height - 6
                radius: Theme.radiusLg
                color: hover.hovered ? Theme.surface2 : "transparent"
                Behavior on color { ColorAnimation { duration: Motion.fast } }
            }
            HoverHandler { id: hover }

            Cover {
                id: cover
                x: Theme.gutter
                anchors.verticalCenter: parent.verticalCenter
                width: 40
                height: 56
                radius: 7
                source: row.modelData.cover || ""
            }
            Column {
                id: details
                anchors.left: cover.right
                anchors.leftMargin: 16
                anchors.right: actions.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text {
                    width: parent.width
                    text: (row.modelData.seriesTitle || "") + " · " + (row.modelData.unitTitle || "")
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Row {
                    id: statusRow
                    width: parent.width
                    spacing: 10
                    Rectangle {
                        id: bar
                        visible: row.modelData.status === "running" || row.modelData.status === "queued"
                        width: Math.min(160, details.width * 0.45)
                        height: 4
                        radius: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.surface3
                        Rectangle {
                            width: parent.width * (row.count.total > 0 ? row.count.done / row.count.total : 0)
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                            Behavior on width { NumberAnimation { duration: Motion.normal } }
                        }
                    }
                    Text {
                        text: row.modelData.status === "running" ? (!row.count.total ? qsTr("Starting…")
                                : row.modelData.media === "anime" ? Math.floor(row.count.done / row.count.total * 100) + "%"
                                : row.count.done + qsTr(" of ") + row.count.total + qsTr(" pages"))
                            : row.modelData.status === "queued" ? qsTr("Waiting")
                            : row.modelData.status === "failed" ? (row.modelData.error || qsTr("Failed"))
                            : qsTr("Saved ") + App.relativeTime(row.modelData.createdAt)
                        color: row.modelData.status === "failed" ? Theme.danger : Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, statusRow.width - (bar.visible ? bar.width + statusRow.spacing : 0))
                    }
                }
            }
            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: Theme.gutter - 6
                anchors.verticalCenter: parent.verticalCenter
                Spinner { visible: row.modelData.status === "running"; size: 16; anchors.verticalCenter: parent.verticalCenter }
                IconButton { visible: row.modelData.status === "failed"; iconName: "refresh-cw"; tip: qsTr("Retry"); onClicked: Downloads.retry(row.modelData.id) }
                IconButton {
                    visible: row.modelData.status === "done"
                    iconName: "book-open"
                    tip: qsTr("Open series")
                    onClicked: Router.openSeries({ media: row.modelData.media, sourceUrl: row.modelData.sourceUrl, title: row.modelData.seriesTitle, cover: row.modelData.cover, source: row.modelData.source, extra: row.modelData.extra || {} }, cover)
                }
                // While it runs: it stops, and what it had goes.
                IconButton { iconName: "trash"; tip: row.modelData.status === "running" ? qsTr("Stop and delete") : qsTr("Delete"); onClicked: Downloads.remove(row.modelData.id) }
            }
        }
    }

    BackToTop { flick: list }
}
