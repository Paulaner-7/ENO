import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Top of a details page: cover (hero target), title, metadata, description
// and primary actions.
Item {
    id: header
    property var series: ({})
    property var anilist: null
    property string kicker: ""
    property var meta: []              // strings
    property string primaryText: qsTr("Start reading")
    property string primaryDetail: ""
    property bool saved: false
    property bool busy: false
    property string coverReferer: ""
    // Cover above the text (phones, and a details column beside episodes).
    property bool stacked: Theme.compact
    readonly property alias coverItem: cover
    default property alias extraActions: actionsRow.data
    signal primary()
    signal toggleSaved()

    width: parent ? parent.width : 900
    implicitHeight: Math.max(info.y + info.height, coverBox.y + coverBox.height) + 34

    IconButton {
        x: Theme.gutter - 8
        y: 20
        iconName: "arrow-left"
        filled: true
        tip: qsTr("Back")
        onClicked: Router.back()
    }

    Item {
        id: coverBox
        x: header.stacked ? (header.width - width) / 2 : Theme.gutter
        y: header.stacked ? 64 : 76
        width: Theme.compact ? 150 : header.stacked ? 190 : header.width > 1100 ? 230 : 190
        height: width * 1.5
        Shadow { target: cover; lift: 1; strength: Theme.dark ? 0.9 : 0.3 }
        Cover {
            id: cover
            anchors.fill: parent
            radius: Theme.radiusLg
            source: header.series.cover || ""
            referer: header.coverReferer
            title: header.series.title || ""
        }
    }

    Column {
        id: info
        x: header.stacked ? Theme.gutter : coverBox.x + coverBox.width + 36
        width: header.width - x - Theme.gutter
        y: header.stacked ? coverBox.y + coverBox.height + 22 : coverBox.y + 14
        spacing: 14

        opacity: 0
        transform: Translate { id: infoRise; y: 16 }
        Component.onCompleted: reveal.start()
        ParallelAnimation {
            id: reveal
            NumberAnimation { target: info; property: "opacity"; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
            NumberAnimation { target: infoRise; property: "y"; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }

        Text {
            visible: text.length > 0
            text: header.kicker
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.4
        }
        Text {
            width: parent.width
            text: header.series.title || ""
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.compact ? 26 : header.stacked ? 30 : header.width > 1100 ? 40 : 32
            font.weight: Font.Bold
            font.letterSpacing: -0.8
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            lineHeight: 1.05
        }
        Flow {
            width: parent.width
            spacing: 8
            Repeater {
                model: header.meta.filter(m => !!m)
                Item {
                    required property var modelData
                    readonly property bool isRating: String(modelData).startsWith("★ ")
                    readonly property bool isAiring: /^(Airing|RELEASING|Ongoing|currently_airing|In corso)$/i.test(String(modelData).trim())
                    readonly property bool isFinished: /^(Finished|FINISHED|Completed|COMPLETED|finished_airing|Finito)$/i.test(String(modelData).trim())
                    readonly property string displayText: isRating ? String(modelData).slice(2)
                        : isAiring ? qsTr("In corso")
                        : isFinished ? qsTr("Finito")
                        : String(modelData)
                    height: 26
                    width: metaRow.implicitWidth + 18
                    LiquidGlass {
                        anchors.fill: parent
                        radius: 13
                        glassStyle: "clear"
                        autoBackdrop: false
                        iconName: parent.isRating ? "star" : ""
                        iconSize: 13
                        iconColor: Theme.warning
                        text: parent.displayText
                        textColor: parent.isAiring
                            ? (Theme.dark ? "#ff6b6b" : Theme.danger)
                            : parent.isRating ? Theme.text : Theme.textMuted
                        fontSize: Theme.textXs + 1
                        bold: parent.isRating || parent.isAiring
                        tintColor: parent.isAiring
                            ? Theme.alpha(Theme.danger, Theme.dark ? 0.32 : 0.25)
                            : parent.isRating
                                ? Theme.alpha(Theme.warning, Theme.dark ? 0.18 : 0.22)
                                : Theme.alpha(Theme.surface2, Theme.dark ? 0.55 : 0.72)
                    }
                    Row {
                        id: metaRow
                        anchors.centerIn: parent
                        spacing: 5
                        Icon {
                            visible: isRating
                            name: "star"
                            size: 13
                            color: Theme.warning
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: displayText
                            color: isAiring ? (Theme.dark ? "#ff6b6b" : Theme.danger) : isRating ? Theme.text : Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textXs + 1
                            font.weight: (isRating || isAiring) ? Font.DemiBold : Font.Medium
                        }
                    }
                }
            }
        }
        Text {
            visible: !!header.series.author
            text: header.series.author || ""
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
        }

        Flow {
            id: actionsRow
            width: parent.width
            spacing: 10
            ScButton {
                variant: "primary"
                text: header.primaryText
                iconName: "play"
                glassBezel: true
                loading: header.busy
                onClicked: header.primary()
            }
            // In the library: the symbol alone, filled once it is saved.
            IconButton {
                diameter: 38
                filled: true
                active: header.saved
                iconName: "bookmark"
                iconColor: header.saved ? Theme.text : Theme.textMuted
                tip: header.saved ? qsTr("In library") : qsTr("Add to library")
                transform: Translate { y: Math.max(0, (38 - height) / 2) }
                onClicked: header.toggleSaved()
            }
            IconButton {
                id: heart
                diameter: 38
                visible: header.saved && !!header.series.id
                readonly property bool on: { App.dependOn(Library.revision); return header.saved && Library.isFavourite(header.series.id) }
                filled: true
                active: on
                iconName: "heart"
                iconColor: on ? Theme.danger : Theme.textMuted
                tip: on ? qsTr("Remove from favourites") : qsTr("Add to favourites")
                transform: Translate { y: Math.max(0, (38 - height) / 2) }
                onClicked: Library.setFavourite(header.series.id, !on)
            }
        }
        Text {
            visible: header.primaryDetail.length > 0
            text: header.primaryDetail
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
        }

        Text {
            id: description
            property bool expanded: false
            width: Math.min(parent.width, 820)
            visible: text.length > 0
            text: header.series.description || ""
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
            lineHeight: 1.45
            wrapMode: Text.Wrap
            maximumLineCount: expanded ? 400 : 4
            elide: Text.ElideRight
            MouseArea {
                anchors.fill: parent
                cursorShape: description.truncated || description.expanded ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: description.expanded = !description.expanded
            }
        }
        Flow {
            width: Math.min(parent.width, 820)
            spacing: 6
            Repeater {
                model: (header.series.genres || []).slice(0, 14)
                Text {
                    required property var modelData
                    text: "#" + String(modelData).toLowerCase().replace(/\s+/g, "-")
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    rightPadding: 6
                }
            }
        }
    }
}
