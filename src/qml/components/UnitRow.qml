import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// One chapter or episode in a details list.
AbstractButton {
    id: row
    property var unit: ({})
    property var progress: null        // {percent, completed}
    property var download: null        // {status, done, total}
    property bool current: false
    property bool selecting: false
    property bool selected: false
    property bool canDownload: true
    property bool bookmarked: false
    // One cell of the chapter grid: a tile, with only the download state at
    // its end (read and download stay in the long-press menu).
    property bool cell: false
    signal toggleRead()
    signal requestDownload()
    signal removeDownload()
    // A right click or a long press: more actions, at (x, y) in the row.
    signal menuRequested(real x, real y)

    readonly property bool done: progress ? progress.completed === true : false
    readonly property real percent: progress ? (progress.percent || 0) : 0

    // Titles may take two lines.
    implicitHeight: Math.max(cell ? 58 : 54, info.implicitHeight + 18)
    height: implicitHeight
    hoverEnabled: true
    onPressAndHold: if (!selecting) menuRequested(width / 2, height / 2)

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => { if (!row.selecting) row.menuRequested(point.position.x, point.position.y) }
    }

    background: Rectangle {
        x: row.cell ? 3 : Theme.gutter - 12
        y: row.cell ? 3 : 0
        width: row.cell ? row.width - 6 : row.width - 2 * Theme.gutter + 24
        height: row.cell ? row.height - 6 : row.height
        radius: Theme.radiusMd
        color: row.selected ? Theme.alpha(Theme.accent, 0.14)
             : row.current ? Theme.alpha(Theme.text, 0.08)
             : row.hovered ? Theme.alpha(Theme.text, row.cell ? 0.07 : 0.04)
             : row.cell ? Theme.alpha(Theme.text, 0.035) : "transparent"
        Behavior on color { ColorAnimation { duration: Motion.fast } }
    }

    contentItem: Item {
        Rectangle {
            visible: row.selecting
            x: row.cell ? 12 : Theme.gutter
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            radius: 5
            color: row.selected ? Theme.accent : "transparent"
            border.width: row.selected ? 0 : 1.5
            border.color: Theme.borderStrong
            Icon {
                anchors.centerIn: parent
                visible: row.selected
                name: "check"
                size: 12
                color: Theme.accentInk
            }
        }

        Column {
            id: info
            x: (row.cell ? 12 : Theme.gutter) + (row.selecting ? 32 : 0)
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - (row.cell ? 8 : Theme.gutter) - trailing.width - (trailing.width > 0 ? 12 : 0)
            spacing: 3
            Behavior on x { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }
            Text {
                width: parent.width
                leftPadding: row.bookmarked ? 20 : 0
                text: row.unit.title || (qsTr("Chapter ") + (row.unit.number || ""))
                color: row.done ? Theme.textFaint : Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: row.current ? Font.DemiBold : Font.Normal
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                Icon {
                    visible: row.bookmarked
                    y: (parent.font.pixelSize * 1.25 - height) / 2
                    name: "bookmark"
                    size: 14
                    color: Theme.accent
                }
            }
            Row {
                spacing: 8
                visible: subtitle.text.length > 0 || progressBar.visible
                Rectangle {
                    id: progressBar
                    visible: !row.done && row.percent > 0.01
                    width: 60
                    height: 3
                    radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.surface3
                    Rectangle {
                        width: parent.width * row.percent
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                    }
                }
                Text {
                    id: subtitle
                    width: Math.min(implicitWidth, info.width - (progressBar.visible ? 68 : 0))
                    elide: Text.ElideRight
                    text: {
                        const parts = []
                        if (row.unit.released) parts.push(App.relativeTime(row.unit.released))
                        if (row.unit.scanlator) parts.push(row.unit.scanlator)
                        if (row.done) parts.push(qsTr("Read"))
                        return parts.join(" · ")
                    }
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs + 1
                }
            }
        }

        Row {
            id: trailing
            anchors.right: parent.right
            anchors.rightMargin: row.cell ? 4 : Theme.gutter - 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Item {
                width: 34
                height: 34
                visible: row.download !== null && row.download.status === "running"
                Spinner { anchors.centerIn: parent; size: 16 }
                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 16
                    text: row.download ? row.download.done + "/" + row.download.total : ""
                    color: Theme.textFaint
                    font.pixelSize: 9
                }
            }
            IconButton {
                visible: row.canDownload && (row.cell ? row.download !== null : (row.hovered || Theme.touch || row.download !== null))
                         && !(row.download && row.download.status === "running")
                iconName: row.download && row.download.status === "done" ? "check"
                        : row.download && row.download.status === "failed" ? "triangle-alert"
                        : row.download && row.download.status === "queued" ? "clock" : "download"
                iconColor: row.download && row.download.status === "done" ? Theme.success
                         : row.download && row.download.status === "failed" ? Theme.danger : Theme.textMuted
                iconSize: 16
                tip: row.download && row.download.status === "done" ? qsTr("Downloaded · click to remove")
                   : row.download && row.download.status === "failed" ? qsTr("Download failed · click to retry") : qsTr("Download")
                onClicked: row.download && row.download.status === "done" ? row.removeDownload() : row.requestDownload()
            }
            IconButton {
                visible: !row.cell && (row.hovered || Theme.touch)
                iconName: row.done ? "eye-off" : "eye"
                iconSize: 16
                tip: row.done ? qsTr("Mark unread") : qsTr("Mark read")
                onClicked: row.toggleRead()
            }
        }
    }
}
