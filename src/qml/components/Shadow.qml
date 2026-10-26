import QtQuick
import Scancore

// Blurred drop shadow drawn from a 9-slice image, cheap enough for grids.
BorderImage {
    id: shadow
    property Item target
    property real lift: 0
    property real strength: Theme.dark ? 0.7 : 0.22

    source: "qrc:/branding/shadow.png"
    border { left: 40; top: 40; right: 40; bottom: 40 }
    x: target ? target.x - 34 : 0
    y: target ? target.y - 30 + 8 + lift * 8 : 0
    width: target ? target.width + 68 : 0
    height: target ? target.height + 68 : 0
    opacity: strength * (0.45 + lift * 0.55)
    Behavior on lift { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }
}
