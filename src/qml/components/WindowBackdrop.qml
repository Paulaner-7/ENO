import QtQuick
import QtQuick.Effects
import Scancore

// Behind everything in the window: the theme's window colour (see-through
// when the custom theme says so), and the custom theme's picture or GIF,
// fitted, optionally blurred, and dimmed with the window colour so text stays
// readable over it.
Item {
    id: backdrop
    // Animated backgrounds pause while nothing shows them.
    property bool playing: true

    readonly property var settings: Theme.customTheme
    readonly property string source: Theme.backgroundSource
    readonly property real dim: Theme.clamp(Number(settings.backgroundDim), 0, 0.95, 0.55)
    readonly property real blur: Theme.clamp(Number(settings.backgroundBlur), 0, 1, 0)
    readonly property int fill: {
        switch (settings.backgroundFit) {
        case "contain": return Image.PreserveAspectFit
        case "tile": return Image.Tile
        case "stretch": return Image.Stretch // 0, so not a lookup with a fallback
        default: return Image.PreserveAspectCrop
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.alpha(Theme.base, Theme.windowOpacity)
        Behavior on color { ColorAnimation { duration: Motion.normal } }
    }

    // Multidirectional bold ASCII fluid animation background. It runs on its
    // own, continuously: it never pauses with the window.
    AsciiFluid {
        id: asciiFluid
        anchors.fill: parent
        visible: Theme.asciiBackground && backdrop.source.length === 0
        blur: Theme.asciiBlur
        dim: Theme.asciiDim
        speed: Theme.asciiSpeed
        fontSize: Theme.asciiFontSize
    }

    Loader {
        id: picture
        anchors.fill: parent
        active: backdrop.source.length > 0
        opacity: status === Loader.Ready && item.ready ? Theme.windowOpacity : 0
        Behavior on opacity { NumberAnimation { duration: Motion.slow } }
        sourceComponent: backdrop.settings.backgroundAnimated === true ? animated : still
    }

    Component {
        id: still
        Item {
            readonly property bool ready: image.status === Image.Ready
            Image {
                id: image
                anchors.fill: parent
                visible: backdrop.blur === 0
                source: backdrop.source
                fillMode: backdrop.fill
                asynchronous: true
                cache: false
                smooth: true
            }
            MultiEffect {
                anchors.fill: parent
                visible: backdrop.blur > 0
                source: image
                autoPaddingEnabled: false
                blurEnabled: true
                blur: backdrop.blur
                blurMax: 64
            }
        }
    }
    Component {
        id: animated
        Item {
            readonly property bool ready: movie.status === AnimatedImage.Ready
            AnimatedImage {
                id: movie
                anchors.fill: parent
                visible: backdrop.blur === 0
                source: backdrop.source
                fillMode: backdrop.fill
                cache: false
                playing: backdrop.playing && !Motion.reduced
            }
            MultiEffect {
                anchors.fill: parent
                visible: backdrop.blur > 0
                source: movie
                autoPaddingEnabled: false
                blurEnabled: true
                blur: backdrop.blur
                blurMax: 64
            }
        }
    }

    // Dims the picture towards the window colour.
    Rectangle {
        anchors.fill: parent
        visible: picture.active
        color: Theme.alpha(Theme.base, backdrop.dim * Theme.windowOpacity)
        Behavior on color { ColorAnimation { duration: Motion.normal } }
    }
}
