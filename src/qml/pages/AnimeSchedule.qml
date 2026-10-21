import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Airing schedule for the next seven days in local time.
Item {
    id: page
    property int day: 0
    readonly property var days: Array.from({ length: 7 }, (_, i) => {
        const d = new Date()
        d.setHours(0, 0, 0, 0)
        d.setDate(d.getDate() + i)
        return d
    })
    property var task: load()

    function load() {
        const start = new Date(days[day])
        const end = new Date(days[day])
        end.setHours(23, 59, 59, 999)
        return Anime.schedule(Math.floor(start.getTime() / 1000), Math.floor(end.getTime() / 1000))
    }
    onDayChanged: task = load()

    SmoothFlickable {
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 22

            PageHeader {
                title: qsTr("Schedule")
                subtitle: qsTr("Episodes airing this week, in your time zone")
                showBack: true
            }

            ChipRow {
                Repeater {
                    model: page.days
                    Chip {
                        required property var modelData
                        required property int index
                        text: index === 0 ? qsTr("Today") : index === 1 ? qsTr("Tomorrow") : Qt.formatDate(modelData, "ddd d")
                        selected: page.day === index
                        onClicked: page.day = index
                    }
                }
            }

            Column {
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 6
                Repeater {
                    model: page.task && page.task.ok ? page.task.result : []
                    delegate: AbstractButton {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool aired: modelData.airingAt * 1000 < Date.now()
                        width: parent.width
                        height: 86
                        hoverEnabled: true
                        onClicked: Router.openSeries(modelData, cover)
                        opacity: 0
                        Component.onCompleted: appear.start()
                        SequentialAnimation {
                            id: appear
                            PauseAnimation { duration: Motion.stagger(row.index) }
                            NumberAnimation { target: row; property: "opacity"; to: row.aired ? 0.6 : 1; duration: Motion.slow }
                        }
                        background: Rectangle {
                            radius: Theme.radiusLg
                            color: row.hovered ? Theme.surface2 : "transparent"
                            Behavior on color { ColorAnimation { duration: Motion.fast } }
                        }
                        contentItem: Row {
                            spacing: 18
                            leftPadding: 10
                            Text {
                                width: 64
                                anchors.verticalCenter: parent.verticalCenter
                                text: Qt.formatTime(new Date(row.modelData.airingAt * 1000), "hh:mm")
                                color: row.aired ? Theme.textFaint : Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: Theme.textXl
                                font.weight: Font.DemiBold
                            }
                            Cover {
                                id: cover
                                width: 46
                                height: 66
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 8
                                source: row.modelData.coverSmall || row.modelData.cover
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: row.width - 10 - 64 - 46 - 2 * 18 - 10
                                spacing: 4
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: row.modelData.title
                                    color: Theme.text
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textMd
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    // No episode number when it is not known (AniList down).
                                    text: (!row.modelData.airingEpisode ? ""
                                          : (row.modelData.airingEpisodeLast
                                             ? qsTr("Episodes ") + row.modelData.airingEpisode + "–" + row.modelData.airingEpisodeLast
                                             : qsTr("Episode ") + row.modelData.airingEpisode)
                                            + (row.modelData.episodes ? qsTr(" of ") + row.modelData.episodes : "") + " · ")
                                        + (row.aired ? qsTr("aired ") : qsTr("airs ")) + App.relativeTime(row.modelData.airingAt)
                                    color: Theme.textMuted
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textSm
                                }
                            }
                        }
                    }
                }
            }

            StateView {
                width: parent.width
                task: page.task
                empty: page.task && page.task.ok && page.task.result.length === 0
                emptyTitle: qsTr("Nothing airs this day")
                emptyIcon: "calendar"
                onRetry: page.task = page.load()
            }
        }
    }
}
