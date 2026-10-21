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
    property string referer: ""
    property string title: item.title || ""
    property string subtitle: ""
    property string badge: ""
    property real progress: -1
    property int index: 0
    property bool animateIn: true
    property real coverRatio: 1.5
    readonly property alias coverItem: cover

    signal opened(var item, Item coverItem)

    implicitWidth: 160
    implicitHeight: width * coverRatio + textColumn.implicitHeight + 10
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    onClicked: opened(item, cover)

    // Entrance: rise and fade in, staggered by index.
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
        PauseAnimation { duration: Motion.stagger(card.index % 24) }
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
                zoom: card.hovered ? 1.06 : 1
                Behavior on zoom { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                scale: card.pressed ? 0.975 : 1
                Behavior on scale { NumberAnimation { duration: Motion.fast } }
            }

            Rectangle {
                anchors.fill: cover
                radius: Theme.radiusMd
                color: "transparent"
                border.width: card.visualFocus ? 2 : 1
                border.color: card.visualFocus ? Theme.accent : Theme.alpha(Theme.text, 0.06)
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
