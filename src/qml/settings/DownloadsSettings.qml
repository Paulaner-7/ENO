import QtQuick
import QtQuick.Controls.Basic
import ScanCore

SettingsPane {
    id: pane
    title: qsTr("Downloads")
    subtitle: (Downloads.items(), page.items.filter(i => i.status === "done").length + qsTr(" saved · ") + Downloads.sizeOnDisk())
    iconName: "download"

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

        property var items: []
        property var progress: ({})
        property string shape: ""
        Connections {
            target: page.task
            function onFinished(result) {
                const visibleResult = Router.reachable(result)
                const counts = {}
                for (const i of visibleResult) counts[i.id] = { done: i.done, total: i.total }
                page.progress = counts
                const shape = JSON.stringify(visibleResult.map(i => [i.id, i.status, i.error]))
                if (shape !== page.shape) {
                    page.shape = shape
                    page.items = visibleResult
                }
            }
        }
        readonly property int savedCount: items.filter(i => i.status === "done").length
        onSavedCountChanged: sizeText = Downloads.sizeOnDisk()
        readonly property int failedCount: items.filter(i => i.status === "failed").length
    }

    SettingsGroup {
        title: qsTr("Queue & Storage")

        SettingRow {
            iconName: "folder"
            title: qsTr("Storage folder")
            description: Downloads.sizeOnDisk() + (App.platform !== "android" ? " · " + Downloads.folder() : "")
            Row {
                spacing: 8
                visible: App.platform !== "android"
                ScButton {
                    text: qsTr("Change folder")
                    iconName: "folder"
                    variant: "ghost"
                    onClicked: downloadFolderDialog.open()
                }
                ScButton {
                    visible: Downloads.hasCustomFolder()
                    text: qsTr("Reset")
                    variant: "ghost"
                    onClicked: {
                        Downloads.resetFolder()
                        Router.toast(qsTr("Reset to default downloads folder"))
                    }
                }
                ScButton {
                    text: qsTr("Open folder")
                    iconName: "folder-open"
                    onClicked: App.openFolder(Downloads.folder())
                }
            }
        }

        SettingRow {
            visible: page.failedCount > 0
            iconName: "alert-circle"
            title: qsTr("Failed downloads (%1)").arg(page.failedCount)
            Row {
                spacing: 8
                ScButton { text: qsTr("Retry failed"); iconName: "refresh-cw"; onClicked: Downloads.retryFailed() }
                ScButton { text: qsTr("Clear failed"); variant: "ghost"; onClicked: Downloads.clearFinished() }
            }
        }

        // Android battery note
        Rectangle {
            visible: App.platform === "android" && App.batteryRestricted
            width: parent.width - 24
            x: 12
            height: visible ? batteryRow.implicitHeight + 20 : 0
            radius: Theme.radiusLg
            color: Theme.surface2
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
                    text: qsTr("Battery optimisation pauses downloads when you leave ENO.")
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    wrapMode: Text.Wrap
                }
                ScButton { id: allowBattery; anchors.verticalCenter: parent.verticalCenter; compact: true; text: qsTr("Allow"); onClicked: App.askBatteryUnrestricted() }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Downloaded episodes")

        StateView {
            width: parent.width
            height: visible ? 200 : 0
            task: page.items.length === 0 ? page.task : null
            empty: page.task && page.task.ok && page.items.length === 0
            emptyTitle: qsTr("No downloads yet")
            emptyText: qsTr("On an anime page, use the download button on an episode, or select several at once. Downloads open without a connection.")
            emptyIcon: "download"
        }

        Repeater {
            model: page.items
            delegate: Item {
                id: row
                required property var modelData
                readonly property var count: page.progress[modelData.id] || { done: modelData.done, total: modelData.total }
                width: parent.width
                height: 74

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    radius: Theme.radiusLg
                    color: hover.hovered ? Theme.surface2 : "transparent"
                    Behavior on color { ColorAnimation { duration: Motion.fast } }
                }
                HoverHandler { id: hover }

                Cover {
                    id: cover
                    x: 12
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
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    Spinner { visible: row.modelData.status === "running"; size: 16; anchors.verticalCenter: parent.verticalCenter }
                    IconButton { visible: row.modelData.status === "failed"; iconName: "refresh-cw"; tip: qsTr("Retry"); onClicked: Downloads.retry(row.modelData.id) }
                    IconButton {
                        visible: row.modelData.status === "done"
                        iconName: "play"
                        tip: qsTr("Open series")
                        onClicked: Router.openSeries({ media: row.modelData.media, sourceUrl: row.modelData.sourceUrl, title: row.modelData.seriesTitle, cover: row.modelData.cover, source: row.modelData.source, extra: row.modelData.extra || {} }, cover)
                    }
                    IconButton { iconName: "trash"; tip: row.modelData.status === "running" ? qsTr("Stop and delete") : qsTr("Delete"); onClicked: Downloads.remove(row.modelData.id) }
                }
            }
        }
    }

    FolderDialog {
        id: downloadFolderDialog
        title: qsTr("Choose downloads folder")
        onAccepted: {
            Downloads.setFolder(selectedFolder)
            Router.toast(qsTr("Downloads folder updated"), "success")
        }
    }
}
