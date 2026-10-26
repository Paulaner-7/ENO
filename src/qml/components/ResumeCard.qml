import QtQuick
import QtQuick.Effects
import Scancore

// The last thing read or watched, large, on Home: its cover over a blurred
// copy of itself, how far along it is, and one click to carry on.
Item {
    id: card
    property var entry: ({})
    readonly property bool anime: entry.media === "anime"
    readonly property bool isFinished: entry.completed && (entry.percent >= 0.96 || (entry.position && entry.position.duration > 0 && entry.position.seconds >= entry.position.duration - 20))
    readonly property real percent: (entry.completed || isFinished) ? 1 : (entry.percent || 0)
    // A finished episode: the next one is what continues.
    readonly property real unitNumber: Number(entry.unitNumber)
    readonly property bool upNext: anime && (entry.completed || isFinished) && unitNumber > 0
    readonly property string unitLine: upNext ? qsTr("Up next: episode %1").arg(Math.floor(unitNumber) + 1)
                                       : anime ? qsTr("Episode ") + (entry.unitNumber || entry.unitKey || "")
                                       : (entry.unitTitle || "")

    width: parent ? parent.width : 800
    implicitHeight: Theme.compact ? 148 : 176

    // Entrance: rise and fade in.
    opacity: 0
    transform: Translate { id: rise; y: 18 }
    Component.onCompleted: reveal.start()
    ParallelAnimation {
        id: reveal
        NumberAnimation { target: card; property: "opacity"; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
        NumberAnimation { target: rise; property: "y"; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
    }

    Shadow {
        target: panel
        lift: hover.hovered ? 0.85 : 0.15
    }

    Rectangle {
        id: panel
        x: Theme.gutter
        y: hover.hovered ? -3 : 0
        width: parent.width - 2 * Theme.gutter
        height: parent.height
        radius: Theme.radiusXl
        color: Theme.surface
        Behavior on y { NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }

        // Cover artwork refracted and frosted through macOS 27 Liquid Glass.
        Image {
            id: backdrop
            anchors.fill: parent
            source: card.entry.cover ? App.image(card.entry.cover) : ""
            sourceSize.width: 240
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: false
        }
        LiquidGlass {
            anchors.fill: parent
            nativeEnabled: false
            radius: panel.radius
            backdropTexture: backdrop
            autoBackdrop: false
            blurMax: 52
            interactive: true
            externalHovered: hover.hovered
            tintColor: Theme.alpha(Theme.surface, hover.hovered ? (Theme.dark ? 0.42 : 0.55) : (Theme.dark ? 0.52 : 0.65))
        }
        Rectangle {
            anchors.fill: parent
            radius: panel.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.alpha(Theme.surface, 0.22) }
                GradientStop { position: 0.68; color: Theme.alpha(Theme.surface, 0.86) }
                GradientStop { position: 1.0; color: Theme.alpha(Theme.surface, 0.96) }
            }
        }

        Shadow {
            target: cover
            lift: hover.hovered ? 0.9 : 0.3
            visible: cover.ready
        }

        Cover {
            id: cover
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - 36
            width: height / 1.5
            radius: Theme.radiusMd
            source: card.entry.cover || ""
            title: card.entry.title || ""
            scale: hover.hovered ? 1.045 : 1
            Behavior on scale { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring } }
        }

        Column {
            anchors.left: cover.right
            anchors.leftMargin: Theme.compact ? 16 : 22
            anchors.right: Theme.compact ? parent.right : actions.left
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Text {
                text: card.anime ? qsTr("Continue watching") : qsTr("Continue reading")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textXs
                font.weight: Font.Bold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1.4
            }
            Text {
                width: parent.width
                text: card.entry.title || ""
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.compact ? Theme.textXl : Theme.text2xl
                font.weight: Font.Bold
                font.letterSpacing: -0.5
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: card.unitLine + (card.entry.progressAt ? "  ·  " + App.relativeTime(card.entry.progressAt) : "")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                elide: Text.ElideRight
            }
            Row {
                topPadding: 6
                spacing: 10
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(220, card.width * 0.3)
                    height: 4
                    radius: 2
                    color: Theme.alpha(Theme.text, 0.12)
                    Rectangle {
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                        width: parent.width * Math.max(0.03, card.percent)
                        Behavior on width { NumberAnimation { duration: Motion.page; easing.type: Easing.OutCubic } }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.upNext ? "" : (card.isFinished || card.entry.completed) ? qsTr("Finished") : Math.round(card.percent * 100) + "%"
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs + 1
                    font.features: { "tnum": 1 }
                }
            }
        }

        Row {
            id: actions
            visible: !Theme.compact
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            ScButton {
                text: qsTr("Details")
                glassBezel: true
                onClicked: Router.openSeries(card.entry, cover)
            }
            ScButton {
                variant: "primary"
                iconName: "play"
                text: qsTr("Continue")
                glassBezel: true
                onClicked: Router.resume(card.entry)
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: panel.radius
            color: "transparent"
            border.width: 1
            border.color: hover.hovered ? Theme.borderStrong : Theme.border
            Behavior on border.color { ColorAnimation { duration: Motion.fast } }
        }

        HoverHandler { id: hover }
        // Phones: the whole card continues.
        TapHandler {
            enabled: Theme.compact
            onTapped: Router.resume(card.entry)
        }
    }
}
