import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// A titled horizontal row of covers with smooth arrow and wheel scrolling,
// or a grid. Inside a PageSection it follows the user's look for it.
Column {
    id: shelf
    property string title
    property string subtitle
    property var items: []
    property bool loading: false
    property string error: ""
    // PageLayout's look for this shelf ({style, size, count, subtitles,
    // progress}); null outside an arranged page.
    property var look: null
    readonly property bool grid: !!look && look.style === "grid"
    readonly property real lookScale: !look ? 1 : look.size === "small" ? 0.8 : look.size === "large" ? 1.25 : 1
    readonly property var shownItems: look && look.count > 0 ? items.slice(0, look.count) : items
    readonly property bool hasContent: loading || error.length > 0 || items.length > 0 || (headerExtra !== null && opacity > 0)
    property int cardWidth: Math.round((Theme.compact ? 118 : 156) * Theme.coverScale * lookScale)
    property real coverRatio: 1.5
    property string actionText: ""
    property var subtitleFor: (item) => ""
    property var badgeFor: (item) => ""
    property var progressFor: (item) => -1
    property Component headerExtra: null
    signal action()
    signal retry()
    signal opened(var item, Item coverItem)

    // As in CoverGrid: only the first fill rises in.
    property bool settled: false
    onItemsChanged: {
        if (items.length > 0 && !settled) Qt.callLater(() => shelf.settled = true)
        if (list) list.contentX = -list.leftMargin
    }

    spacing: 20
    width: parent ? parent.width : 800
    visible: hasContent

    Item {
        width: parent.width
        height: 30
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.gutter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            Text {
                // Elide rather than run under the buttons on narrow screens.
                width: Math.min(implicitWidth, shelf.width - Theme.gutter - actions.width - Theme.gutter)
                elide: Text.ElideRight
                text: shelf.title
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textXl
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                visible: shelf.subtitle.length > 0
                text: shelf.subtitle
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 2
            }
        }
        Row {
            id: actions
            anchors.right: parent.right
            anchors.rightMargin: Theme.gutter - 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            ScButton {
                visible: shelf.actionText.length > 0
                text: shelf.actionText
                variant: "ghost"
                compact: true
                onClicked: shelf.action()
            }
            IconButton {
                visible: !Theme.compact && !shelf.grid
                iconName: "chevron-left"
                enabled: list.contentX > 1
                onClicked: list.scrollBy(-list.width * 0.8)
            }
            IconButton {
                visible: !Theme.compact && !shelf.grid
                iconName: "chevron-right"
                enabled: list.contentX < list.contentWidth - list.width - 1
                onClicked: list.scrollBy(list.width * 0.8)
            }
        }
    }

    Loader {
        width: parent.width
        active: shelf.headerExtra !== null
        visible: active
        sourceComponent: shelf.headerExtra
    }

    // Grid: every card at once, wrapped.
    Flow {
        visible: shelf.grid && !shelf.loading && shelf.error.length === 0
        x: Theme.gutter
        width: parent.width - 2 * Theme.gutter
        spacing: 18
        Repeater {
            model: shelf.grid ? shelf.shownItems : []
            delegate: CoverCard {
                required property var modelData
                required property int index
                width: shelf.cardWidth
                height: shelf.cardWidth * shelf.coverRatio + 84
                item: modelData
                order: index
                animateIn: !shelf.settled
                coverRatio: shelf.coverRatio
                subtitle: shelf.look && !shelf.look.subtitles ? "" : shelf.subtitleFor(modelData)
                badge: shelf.badgeFor(modelData)
                progress: shelf.look && !shelf.look.progress ? -1 : shelf.progressFor(modelData)
                onOpened: (it, cover) => shelf.opened(it, cover)
            }
        }
    }

    Item {
        width: parent.width
        visible: (!shelf.grid || shelf.loading || shelf.error.length > 0) && height > 0
        height: shelf.items.length > 0 || shelf.loading || shelf.error.length > 0 ? (shelf.cardWidth * shelf.coverRatio + 84) : (shelf.headerExtra !== null ? 48 : 0)

        Text {
            visible: !shelf.grid && shelf.items.length === 0 && !shelf.loading && shelf.error.length === 0 && shelf.headerExtra !== null
            anchors.left: parent.left
            anchors.leftMargin: Theme.gutter
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("No titles in this category")
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
        }

        ListView {
            id: list
            x: 0
            y: -22
            width: parent.width
            height: parent.height + 44
            visible: !shelf.grid && shelf.items.length > 0 && !shelf.loading && shelf.error.length === 0
            orientation: ListView.Horizontal
            flickableDirection: Flickable.HorizontalFlick
            leftMargin: Theme.gutter
            rightMargin: Theme.gutter
            spacing: 18
            clip: true
            model: shelf.grid ? [] : shelf.shownItems
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: 4000
            reuseItems: true
            cacheBuffer: 600
            // Covers that scroll into view later appear at once; the
            // staggered entrance is for the first screenful.
            property bool scrolled: false
            onContentXChanged: if (contentX > originX - leftMargin + 2) scrolled = true

            function scrollBy(delta) {
                const target = Math.max(-leftMargin, Math.min(contentWidth - width + rightMargin + originX, contentX + delta))
                scroller.to = target
                scroller.restart()
            }
            NumberAnimation {
                id: scroller
                target: list
                property: "contentX"
                duration: Motion.slow + 120
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Motion.emphasized
            }
            // Sideways wheels, and Shift with an ordinary wheel, scroll the row;
            // a plain wheel must reach the page. A WheelHandler takes every
            // event it is offered before onWheel runs (setting
            // event.accepted = false there changes nothing), so each one is
            // only offered the events it uses: with the cursor over a row,
            // the page used not to scroll at all.
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse
                orientation: Qt.Horizontal
                onWheel: (event) => list.scrollBy(-event.angleDelta.x * 1.4)
            }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse
                acceptedModifiers: Qt.ShiftModifier
                onWheel: (event) => list.scrollBy(-(event.angleDelta.y || event.angleDelta.x) * 1.4)
            }

            delegate: Item {
                id: slot
                required property var modelData
                required property int index
                width: shelf.cardWidth
                height: list.height
                z: card.hovered || card.pressed ? 10 : 0
                CoverCard {
                    id: card
                    y: 22
                    width: parent.width
                    height: parent.height - 44
                    item: slot.modelData
                    order: list.scrolled ? 0 : slot.index
                    animateIn: !shelf.settled
                    coverRatio: shelf.coverRatio
                    subtitle: shelf.look && !shelf.look.subtitles ? "" : shelf.subtitleFor(slot.modelData)
                    badge: shelf.badgeFor(slot.modelData)
                    progress: shelf.look && !shelf.look.progress ? -1 : shelf.progressFor(slot.modelData)
                    onOpened: (it, cover) => shelf.opened(it, cover)
                }
            }
        }

        // Desktop: the row fades out at an edge with more to scroll to (not over
        // a background picture, which has no single colour to fade into).
        Rectangle {
            visible: list.visible && !Theme.compact && !Theme.layered
            anchors.left: parent.left
            y: -22
            width: Theme.gutter + 24
            height: parent.height + 44
            opacity: list.contentX > list.originX - list.leftMargin + 4 ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Motion.normal } }
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.bg }
                GradientStop { position: 0.45; color: Theme.alpha(Theme.bg, 0.85) }
                GradientStop { position: 1.0; color: Theme.alpha(Theme.bg, 0) }
            }
        }
        Rectangle {
            visible: list.visible && !Theme.compact && !Theme.layered
            anchors.right: parent.right
            y: -22
            width: Theme.gutter + 24
            height: parent.height + 44
            opacity: list.contentWidth > list.width && list.contentX < list.originX + list.contentWidth - list.width + list.rightMargin - 4 ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Motion.normal } }
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.alpha(Theme.bg, 0) }
                GradientStop { position: 0.55; color: Theme.alpha(Theme.bg, 0.85) }
                GradientStop { position: 1.0; color: Theme.bg }
            }
        }

        Row {
            visible: shelf.loading
            x: Theme.gutter
            spacing: 18
            Repeater {
                model: 8
                Column {
                    spacing: 10
                    Shimmer { width: shelf.cardWidth; height: shelf.cardWidth * shelf.coverRatio }
                    Shimmer { width: shelf.cardWidth * 0.8; height: 12; radius: 6 }
                    Shimmer { width: shelf.cardWidth * 0.5; height: 10; radius: 5 }
                }
            }
        }

        StateView {
            anchors.fill: parent
            anchors.leftMargin: Theme.gutter
            anchors.rightMargin: Theme.gutter
            visible: !shelf.loading && shelf.error.length > 0
            error: shelf.error
            compact: true
            onRetry: shelf.retry()
        }
    }
}
