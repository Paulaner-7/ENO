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
    signal toggleRead()
    signal requestDownload()
    signal removeDownload()
    // A right click or a long press: more actions, at (x, y) in the row.
    signal menuRequested(real x, real y)

    readonly property bool done: progress ? progress.completed === true : false
    readonly property real percent: progress ? (progress.percent || 0) : 0

    height: 54
    hoverEnabled: true
    onPressAndHold: if (!selecting) menuRequested(width / 2, height / 2)

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => { if (!row.selecting) row.menuRequested(point.position.x, point.position.y) }
    }

    background: Rectangle {
        x: Theme.gutter - 12
        width: row.width - 2 * Theme.gutter + 24
        radius: Theme.radiusMd
        color: row.selected ? Theme.alpha(Theme.accent, 0.14)
             : row.current ? Theme.alpha(Theme.text, 0.06)
             : row.hovered ? Theme.alpha(Theme.text, 0.04) : "transparent"
        Behavior on color { ColorAnimation { duration: Motion.fast } }
    }

    contentItem: Item {
        Rectangle {
            visible: row.selecting
            x: Theme.gutter
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
            x: Theme.gutter + (row.selecting ? 32 : 0)
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - Theme.gutter - trailing.width - 12
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
                elide: Text.ElideRight
                Icon {
                    visible: row.bookmarked
                    anchors.verticalCenter: parent.verticalCenter
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
            anchors.rightMargin: Theme.gutter - 6
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
                visible: row.canDownload && (row.hovered || Theme.touch || row.download !== null) && !(row.download && row.download.status === "running")
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
                visible: row.hovered || Theme.touch
                iconName: row.done ? "eye-off" : "eye"
                iconSize: 16
                tip: row.done ? qsTr("Mark unread") : qsTr("Mark read")
                onClicked: row.toggleRead()
            }
        }
    }
}
