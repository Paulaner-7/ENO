import QtQuick
import QtQuick.Controls.Basic
import Scancore

// A big toggle for one medium on the welcome's first step: a picture of it
// (covers, a page of text, a video frame), its name and what it covers.
// Retro draws a Windows Phone tile.
AbstractButton {
    id: media
    property string kind: "comic"           // "comic" | "novel" | "anime"
    property string iconName
    property string detail
    property bool on: true
    property WelcomeCoverSource covers
    checkable: false
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    Accessible.role: Accessible.CheckBox
    Accessible.checked: on
    Accessible.name: text + ". " + detail

    // Tall: picture above the words (wide screens); side: picture at the
    // left of a row (phones); tile: Retro's small square tiles.
    readonly property bool tile: Theme.retro && width < height * 1.3
    readonly property bool tall: !tile && height > 150 && width > 200 && width / height < 1.9
    readonly property bool side: !tall && !tile && height >= 96
    readonly property color ink: Theme.retro && on ? Theme.tileInk : Theme.text

    background: Rectangle {
        radius: Theme.retro ? 0 : Theme.radiusXl
        color: Theme.retro ? (media.on ? Theme.tile : Theme.surface2)
             : media.on ? Theme.alpha(Theme.accent, Theme.dark ? 0.10 : 0.07) : media.hovered ? Theme.surface2 : Theme.surface
        border.width: Theme.retro ? 0 : (media.on ? 2 : 1)
        border.color: media.on ? Theme.accent : Theme.border
        scale: media.pressed ? 0.98 : 1
        Behavior on scale { NumberAnimation { duration: Motion.fast } }
        Behavior on color { ColorAnimation { duration: Motion.normal } }
    }

    contentItem: Item {
        // The picture.
        Item {
            id: picture
            visible: media.tall || media.side
            x: media.side ? -media.leftPadding + 12 : 0
            y: media.side ? -media.topPadding + 12 : 0
            width: media.side ? 92 : parent.width
            height: media.side ? media.height - 24 : parent.height - captions.height - 14
            clip: true
            Row {
                visible: media.kind === "comic"
                anchors.centerIn: parent
                spacing: picture.width * 0.04
                Repeater {
                    model: 3
                    WelcomeArt {
                        required property int index
                        width: Math.min(picture.width / 3.6, picture.height / 1.5)
                        height: width * 1.45
                        y: index === 1 ? -height * 0.08 : height * 0.05
                        rotation: (index - 1) * 5
                        seed: 3 + index * 4
                        source: media.covers ? media.covers.pick(index + 1, "comic") : ""
                    }
                }
            }
            // A page of a novel, lines of text on paper.
            Rectangle {
                visible: media.kind === "novel"
                anchors.centerIn: parent
                width: Math.min(picture.width * 0.6, picture.height * 0.78)
                height: picture.height
                radius: Theme.retro ? 0 : 6
                color: Theme.dark ? "#ece4d4" : "#f6efe1"
                Column {
                    x: parent.width * 0.12
                    y: parent.width * 0.12
                    width: parent.width * 0.76
                    spacing: Math.max(3, parent.height * 0.035)
                    Rectangle { width: parent.width * 0.55; height: Math.max(4, parent.parent.height * 0.035); radius: 2; color: "#3b3326" }
                    Repeater {
                        model: Math.max(0, Math.floor((picture.height * 0.8) / Math.max(7, picture.height * 0.06)))
                        Rectangle {
                            required property int index
                            width: parent.width * [1, 0.94, 0.98, 0.7, 1, 0.9][index % 6]
                            height: Math.max(2, picture.height * 0.02)
                            radius: 1
                            color: "#8a7f6c"
                        }
                    }
                }
            }
            // An episode playing, a cover as its frame.
            Rectangle {
                visible: media.kind === "anime"
                anchors.centerIn: parent
                width: Math.min(picture.width, picture.height * 1.6)
                height: width / 1.78
                radius: Theme.retro ? 0 : 8
                clip: true
                color: "#000000"
                WelcomeArt {
                    anchors.fill: parent
                    seed: 9
                    corner: 0
                    source: media.covers ? media.covers.pick(2, "anime") : ""
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(40, parent.height * 0.4); height: width; radius: width / 2
                    color: Qt.rgba(0, 0, 0, 0.5)
                    Icon { anchors.centerIn: parent; name: "play"; size: parent.width * 0.45; color: "#ffffff" }
                }
                Rectangle { x: 8; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; width: parent.width - 16; height: 3; radius: 2; color: Qt.rgba(1, 1, 1, 0.3) }
                Rectangle { x: 8; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; width: (parent.width - 16) * 0.38; height: 3; radius: 2; color: Theme.retro ? Theme.tile : Theme.accent }
            }
        }
        Icon {
            visible: media.tile
            name: media.iconName
            size: 40
            color: media.ink
        }
        Row {
            id: captions
            x: media.side ? picture.width + 16 - media.leftPadding + 12 : 0
            width: parent.width - x - checkMark.width - 8
            y: media.tall || media.tile ? parent.height - height : (parent.height - height) / 2
            spacing: 14
            Rectangle {
                visible: !media.tall && !media.side && !media.tile
                anchors.verticalCenter: parent.verticalCenter
                width: 44; height: 44; radius: Theme.retro ? 0 : 12
                color: Theme.retro ? "transparent" : Theme.surface2
                Icon { anchors.centerIn: parent; name: media.iconName; size: 22; color: media.ink }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - (media.tall || media.side || media.tile ? 0 : 58)
                spacing: 2
                Text {
                    width: parent.width
                    text: media.text
                    color: media.ink
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.retro ? 22 : Theme.textXl
                    font.weight: Theme.retro ? Font.Normal : Font.DemiBold
                    font.capitalization: Theme.retro ? Font.AllLowercase : Font.MixedCase
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: media.detail
                    color: Theme.retro && media.on ? Theme.alpha(Theme.tileInk, 0.85) : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    wrapMode: Text.Wrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                }
            }
        }
        // Chosen or not.
        Rectangle {
            id: checkMark
            visible: !Theme.retro || media.on
            x: parent.width - width
            y: media.tall || media.tile ? 0 : (parent.height - height) / 2
            width: 26; height: 26; radius: Theme.retro ? 0 : 13
            color: media.on && !Theme.retro ? Theme.accent : "transparent"
            border.width: media.on ? 0 : 2
            border.color: Theme.borderStrong
            Icon { visible: media.on; anchors.centerIn: parent; name: "check"; size: 15; color: Theme.retro ? media.ink : Theme.accentInk }
        }
    }
    padding: 16
}
