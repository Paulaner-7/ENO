import QtQuick
import ScanCore

// Rounded cover art. Shows a soft placeholder while loading, then fades and
// settles the image in.
Item {
    id: root
    property string source
    property string referer
    property string title
    property int radius: Theme.radiusMd
    property real zoom: 1
    property color tint: Theme.surface2
    readonly property bool ready: image.status === Image.Ready
    property int decodeWidth: 200
    readonly property real aspect: width > 0 && height > 0 ? Math.round((height / width) * 20) / 20 : 1.5
    readonly property int texW: Math.round(decodeWidth * Screen.devicePixelRatio)
    readonly property int texH: Math.max(1, Math.round(texW * aspect))
    readonly property int texRadius: aspect === 1 && width > 0 && radius * 2 >= width
        ? Math.round(texW / 2)
        : Math.round(texW * 0.0625)

    clip: false

    Rectangle {
        id: placeholder
        anchors.fill: parent
        radius: root.radius
        color: root.tint
        visible: !root.ready || image.opacity < 0.99
        Text {
            anchors.fill: parent
            anchors.margins: 12
            visible: image.status === Image.Error || root.source.length === 0
            text: root.title
            color: Theme.textFaint
            font.family: Theme.fontDisplay
            font.pixelSize: 13
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Shimmer {
            anchors.fill: parent
            radius: root.radius
            visible: image.status === Image.Loading
        }
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.source ? App.image(root.source, root.referer) + "~" + root.texRadius : ""
        sourceSize: Qt.size(root.texW, root.texH)
        fillMode: Image.Stretch
        asynchronous: true
        cache: true
        smooth: true
        mipmap: false
        opacity: root.ready ? 1 : 0
        scale: (root.ready ? 1 : 1.03) * (root.zoom > 1 ? 1 + (root.zoom - 1) * 0.35 : 1)
        Behavior on opacity { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Motion.page; easing.type: Easing.OutCubic } }
    }
}
