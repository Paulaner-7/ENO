import QtQuick
import QtQuick.Controls.Basic
import Scancore

// One Retro tile. `tile` describes it:
//   { key, size: "small" | "medium" | "wide", kind: "cover" | "action" | "icon",
//     label, count, icon, item (a library/search map for covers),
//     back: { label, detail } (a live tile's other side, shown in turns) }
// Pressing tilts it towards the finger, as Windows Phone's tiles did, instead
// of changing colour. Live tiles flip on the hub's shared clock (`beat`), so
// a screen of them costs one timer.
AbstractButton {
    id: tileButton
    property var tile: ({})
    // The hub's shared clock: each step flips the tiles whose turn it is.
    property int beat: 0
    property bool live: true
    // 0 for a tile at rest; the hub's turnstile swings it out to 90.
    property real turn: 0
    // Where the tile sits in the grid (column), for the turnstile's stagger.
    property int column: 0

    readonly property string size: tile.size || "small"
    readonly property string kind: tile.kind || "cover"
    readonly property var item: tile.item || ({})
    readonly property bool hasBack: !!tile.back && size !== "small"
    // Every tile turns on its own beat, so they never flip all at once.
    readonly property bool showBack: live && hasBack && !Motion.reduced && ((beat + (tile.phase || 0)) % 3 === 1)

    signal held()

    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: tile.label || item.title || ""
    onPressAndHold: held()

    // Tilt: towards the press point, less on big tiles.
    property point pressPoint: Qt.point(width / 2, height / 2)
    readonly property real tiltMax: size === "small" ? 12 : size === "medium" ? 8 : 5
    readonly property real tiltX: pressed && !Motion.reduced ? (pressPoint.y - height / 2) / (height / 2) * tiltMax : 0
    readonly property real tiltY: pressed && !Motion.reduced ? -(pressPoint.x - width / 2) / (width / 2) * tiltMax : 0
    onPressed: pressPoint = Qt.point(pressX, pressY)

    transform: [
        Rotation {
            origin.x: tileButton.width / 2
            origin.y: tileButton.height / 2
            axis { x: 1; y: 0; z: 0 }
            angle: tileButton.tiltX
            Behavior on angle { NumberAnimation { duration: Motion.instant } }
        },
        Rotation {
            origin.x: tileButton.width / 2
            origin.y: tileButton.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: tileButton.tiltY
            Behavior on angle { NumberAnimation { duration: Motion.instant } }
        },
        Scale {
            origin.x: tileButton.width / 2
            origin.y: tileButton.height / 2
            xScale: tileButton.pressed && !Motion.reduced ? 0.97 : 1
            yScale: xScale
        },
        // Turnstile: about the left edge of the screen, like a door.
        Rotation {
            origin.x: -tileButton.x
            origin.y: tileButton.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: tileButton.turn
        }
    ]
    opacity: turn > 80 ? 0 : 1

    background: null
    contentItem: Flipable {
        id: flip
        front: face
        back: backFace
        transform: Rotation {
            id: flipTurn
            origin.x: flip.width / 2
            origin.y: flip.height / 2
            axis { x: 1; y: 0; z: 0 }
            angle: tileButton.showBack ? 180 : 0
            Behavior on angle { NumberAnimation { duration: Motion.slow * 2; easing.type: Easing.InOutCubic } }
        }

        Item {
            id: face
            anchors.fill: parent

            Rectangle {
                anchors.fill: parent
                color: tileButton.kind === "cover" ? Theme.surface2 : Theme.tile
            }
            Cover {
                visible: tileButton.kind === "cover"
                anchors.fill: parent
                radius: 0
                source: tileButton.kind === "cover" ? (tileButton.item.cover || "") : ""
                referer: tileButton.item.referer
                    || (tileButton.item.sourceUrl
                        ? (tileButton.item.media === "comic" ? Comics.imageReferer(tileButton.item.sourceUrl)
                            : tileButton.item.media === "novel" ? Novels.imageReferer(tileButton.item.sourceUrl) : "") : "")
                title: tileButton.item.title || ""
                tint: Theme.surface2
            }
            // The title on medium and wide cover tiles, over a shade.
            Rectangle {
                visible: tileButton.kind === "cover" && tileButton.size !== "small" && labelRow.visible
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: labelRow.height + 28
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.72) }
                }
            }
            Icon {
                visible: tileButton.kind === "icon" && !!tileButton.tile.icon
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: tileButton.size === "small" ? 0 : -parent.height * 0.08
                name: tileButton.tile.icon || "square"
                size: Math.round(Math.min(parent.width, parent.height) * (tileButton.size === "small" ? 0.42 : 0.34))
                color: Theme.tileInk
            }
            // A count at the top right of small icon tiles (new chapters,
            // downloads).
            Text {
                visible: tileButton.size === "small" && (tileButton.tile.count || 0) > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 6
                text: tileButton.tile.count || ""
                color: tileButton.kind === "cover" ? "white" : Theme.tileInk
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                style: tileButton.kind === "cover" ? Text.Outline : Text.Normal
                styleColor: Qt.rgba(0, 0, 0, 0.5)
            }
            Item {
                id: labelRow
                visible: tileButton.size !== "small" && (label.text.length > 0 || count.text.length > 0)
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 10
                height: Math.max(label.height, count.height)
                Text {
                    id: label
                    anchors.left: parent.left
                    anchors.right: count.left
                    anchors.rightMargin: 8
                    anchors.bottom: parent.bottom
                    text: tileButton.tile.label !== undefined ? tileButton.tile.label : (tileButton.item.title || "")
                    color: tileButton.kind === "cover" ? "white" : Theme.tileInk
                    font.family: Theme.fontSans
                    // Action tiles read as words ("read a random comic").
                    font.pixelSize: tileButton.kind === "action"
                        ? Math.max(Theme.textLg, Math.round(tileButton.height * 0.15)) : Theme.textLg
                    font.weight: tileButton.kind === "action" ? 350 : Font.Normal
                    lineHeight: 0.92
                    wrapMode: Text.WordWrap
                    maximumLineCount: tileButton.kind === "action" ? 3 : 2
                    elide: Text.ElideRight
                }
                Text {
                    id: count
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    text: tileButton.tile.count || ""
                    color: label.color
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textLg
                }
            }
        }

        // A live tile's other side: the accent, with words.
        Rectangle {
            id: backFace
            anchors.fill: parent
            visible: tileButton.hasBack
            color: Theme.tile
            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                text: tileButton.tile.back ? (tileButton.tile.back.detail || "") : ""
                color: Theme.tileInk
                font.family: Theme.fontSans
                font.pixelSize: Math.max(Theme.textLg, Math.round(tileButton.height * 0.13))
                font.weight: 350
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }
            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 10
                text: tileButton.tile.back ? (tileButton.tile.back.label || "") : ""
                color: Theme.tileInk
                font.family: Theme.fontSans
                font.pixelSize: Theme.textLg
                elide: Text.ElideRight
            }
        }
    }

    // Keyboard focus: a frame, as the phones showed for keyboards.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        color: "transparent"
        border.width: 2
        border.color: Theme.text
        visible: tileButton.visualFocus
    }
}
