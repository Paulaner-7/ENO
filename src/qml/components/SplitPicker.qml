import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Side by side (wide windows): what to read beside the open reader, from
// the comics and novels in progress. The pick opens at the chapter it is
// on, on the right half (Router.toSplit), without closing the reader
// already open.
Dialog {
    id: dialog
    property var comics: null
    property var novels: null
    property var loading: null
    property string opening: ""
    readonly property var items: {
        const all = [].concat(comics && comics.ok ? comics.result : [], novels && novels.ok ? novels.result : [])
        return all.sort((a, b) => (b.progressAt || 0) - (a.progressAt || 0)).slice(0, 30)
    }

    function start() {
        comics = Library.continueItems("comic", 20)
        novels = Library.continueItems("novel", 20)
        opening = ""
        open()
    }
    function pick(entry) {
        opening = entry.title
        loading = entry.media === "novel" ? Novels.book(entry.sourceUrl, entry) : Comics.series(entry.sourceUrl, entry)
        loading.finished.connect((result) => {
            const chapters = result.chapters || []
            if (chapters.length === 0) { Router.toast(qsTr("No chapters to read"), "info"); dialog.opening = ""; return }
            const at = Math.max(0, chapters.findIndex(c => c.key === entry.unitKey))
            Router.toSplit = true
            if (entry.media === "novel") Router.openNovelReader(result.series, chapters, at)
            else Router.openComicReader(result.series, chapters, at)
            dialog.close()
        })
        loading.failed.connect((error) => { Router.toast(error, "error"); dialog.opening = "" })
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, (parent ? parent.width : 520) - 32)
    height: Math.min(560, (parent ? parent.height : 560) - 64)
    modal: true
    focus: true
    padding: 20
    topPadding: 4
    title: ""
    Overlay.modal: Rectangle { color: Theme.overlay }
    background: Rectangle { radius: Theme.radiusXl; color: Theme.surface; border.width: 1; border.color: Theme.borderStrong }

    header: Item {
        implicitHeight: 60
        Text {
            x: 20
            y: 20
            text: qsTr("Read beside")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textLg
            font.weight: Font.DemiBold
        }
        IconButton { anchors.right: parent.right; anchors.rightMargin: 12; y: 12; iconName: "x"; tip: qsTr("Close"); onClicked: dialog.close() }
    }

    contentItem: Item {
        Text {
            anchors.centerIn: parent
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: dialog.items.length === 0 && !!dialog.comics && dialog.comics.done
            text: qsTr("Nothing in progress yet: start a comic or a novel, then read it beside another.")
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            wrapMode: Text.Wrap
        }
        ListView {
            id: list
            anchors.fill: parent
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            maximumFlickVelocity: Motion.flickVelocity
            flickDeceleration: Motion.flickDeceleration
            model: dialog.items
            WheelScroll {}
            ScrollBar.vertical: ScScrollBar {}
            delegate: AbstractButton {
                id: row
                required property var modelData
                width: list.width
                height: 64
                hoverEnabled: true
                enabled: dialog.opening === ""
                onClicked: dialog.pick(modelData)
                background: Rectangle { radius: Theme.radiusMd; color: row.hovered ? Theme.surface2 : "transparent" }
                contentItem: Row {
                    spacing: 12
                    leftPadding: 6
                    Cover {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 38
                        height: 54
                        source: row.modelData.cover || ""
                        title: row.modelData.title || ""
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: row.width - 70
                        spacing: 2
                        Text {
                            width: parent.width
                            text: row.modelData.title || ""
                            color: Theme.text
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textSm
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: dialog.opening === row.modelData.title ? qsTr("Opening…")
                                : (row.modelData.media === "novel" ? qsTr("Novel") : qsTr("Comic")) + (row.modelData.unitTitle ? " · " + row.modelData.unitTitle : "")
                            color: Theme.textFaint
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textXs + 1
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
