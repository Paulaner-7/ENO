import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// A cover with title and subtitle. Lifts and sharpens on hover, settles in
// with a short staggered entrance, and reports its cover for the hero
// transition when opened.
AbstractButton {
    id: card
    property var item: ({})
    property string coverUrl: item.cover || ""
    // Library and history entries carry no referer: their source gives it
    // (sites that refuse covers without one, and extensions, which fetch
    // covers themselves).
    property string referer: item.referer
        || (item.sourceUrl
            ? (item.media === "comic" ? Comics.imageReferer(item.sourceUrl) : item.media === "novel" ? Novels.imageReferer(item.sourceUrl) : "") : "")
    property string title: item.title || ""
    property string subtitle: ""
    property string badge: ""
    property real progress: -1
    // Picked in a selection (the library's Select).
    property bool marked: false
    // Place in the entrance's stagger.
    property int order: 0
    property bool animateIn: true
    property real coverRatio: 1.5
    readonly property alias coverItem: cover

    signal opened(var item, Item coverItem)

    implicitWidth: 160
    implicitHeight: width * coverRatio + textColumn.implicitHeight + 10
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    onClicked: opened(item, cover)

    // Entrance: rise and fade in, staggered by order.
    opacity: 0
    transform: Translate { id: rise; y: 14 }
    Component.onCompleted: {
        if (!animateIn || Motion.reduced || index >= 24) {
            opacity = 1
            rise.y = 0
            return
        }
        entrance.start()
    }
    SequentialAnimation {
        id: entrance
        PauseAnimation { duration: Motion.stagger(card.order % 24) }
        ParallelAnimation {
            NumberAnimation { target: card; property: "opacity"; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
            NumberAnimation { target: rise; property: "y"; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }

    background: null
    contentItem: Item {
        Item {
            id: coverWrap
            width: parent.width
            height: width * card.coverRatio
            y: card.hovered ? -4 : 0
            Behavior on y { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }

            // Soft shadow that deepens and spreads on hover.
            Shadow {
                target: cover
                lift: card.hovered ? 1 : 0
                visible: cover.ready
            }

            Cover {
                id: cover
                anchors.fill: parent
                source: card.coverUrl
                referer: card.referer
                title: card.title
                radius: Theme.radiusMd
                // A saved series whose site moved its pictures.
                onFailedChanged: if (failed && card.item.id && card.item.progressAt !== undefined) Library.refreshCover(card.item.id)
                zoom: card.hovered ? 1.06 : 1
                Behavior on zoom { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                scale: card.pressed ? 0.975 : 1
                Behavior on scale { NumberAnimation { duration: Motion.fast } }
            }

            Rectangle {
                anchors.fill: cover
                radius: Theme.radiusMd
                color: "transparent"
                border.width: card.visualFocus || card.marked ? 2 : 1
                border.color: card.visualFocus || card.marked ? Theme.accent : Theme.alpha(Theme.text, 0.06)
            }

            // Selected: a wash and a check in the corner.
            Rectangle {
                visible: card.marked
                anchors.fill: cover
                radius: Theme.radiusMd
                color: Theme.alpha(Theme.accent, 0.22)
            }
            Rectangle {
                visible: card.marked
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 8
                width: 24
                height: 24
                radius: 12
                color: Theme.accent
                Icon { anchors.centerIn: parent; name: "check"; size: 14; color: Theme.accentInk }
            }

            Rectangle {
                visible: card.badge.length > 0
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                height: 22
                width: badgeText.implicitWidth + 14
                radius: 11
                color: Theme.accent
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: card.badge
                    color: Theme.accentInk
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            Rectangle {
                visible: card.progress >= 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 8
                height: 4
                radius: 2
                color: Theme.alpha("#000000", 0.45)
                Rectangle {
                    width: parent.width * Math.max(0.04, Math.min(1, card.progress))
                    height: parent.height
                    radius: 2
                    color: "white"
                }
            }
        }

        Column {
            id: textColumn
            anchors.top: coverWrap.bottom
            anchors.topMargin: 10
            width: parent.width
            spacing: 2
            Text {
                width: parent.width
                text: card.title
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                lineHeight: 1.1
            }
            Text {
                visible: text.length > 0
                width: parent.width
                text: card.subtitle
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                elide: Text.ElideRight
            }
        }
    }
}
