import QtQuick
import Scancore

// One cover on the welcome: a real one when it has come (`source`), drawn
// colours and shapes under it until then, or offline.
Item {
    id: art
    property int seed: 0
    property string source
    property real corner: Theme.retro ? 0 : Math.max(3, width * 0.05)
    readonly property bool real: image.status === Image.Ready
    readonly property var hue: [[0.02, 0.92], [0.58, 0.70], [0.78, 0.62], [0.33, 0.48], [0.10, 0.02], [0.50, 0.60],
                               [0.68, 0.84], [0.08, 0.14], [0.95, 0.75], [0.45, 0.55], [0.00, 0.08], [0.62, 0.52]][Math.abs(seed) % 12]

    Rectangle {
        anchors.fill: parent
        radius: art.corner
        clip: true
        visible: image.opacity < 1
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.hsla(art.hue[0], 0.5, 0.42, 1) }
            GradientStop { position: 1; color: Qt.hsla(art.hue[1], 0.6, 0.15, 1) }
        }
        Rectangle {
            width: art.width * [0.9, 0.6, 1.2, 0.7][Math.abs(art.seed) % 4]
            height: width
            radius: width / 2
            x: art.width * [0.35, -0.1, 0.1, 0.45][Math.abs(art.seed) % 4]
            y: art.height * [0.08, 0.2, -0.25, 0.3][Math.abs(art.seed) % 4]
            color: Qt.hsla(art.hue[1], 0.7, 0.68, 0.35)
        }
    }
    Image {
        id: image
        anchors.fill: parent
        source: art.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        smooth: true
        sourceSize.width: Math.ceil(art.width * Screen.devicePixelRatio)
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Motion.slow } }
    }
}
