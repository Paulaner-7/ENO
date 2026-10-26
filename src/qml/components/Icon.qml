import QtQuick
import Scancore

// A Lucide icon tinted to a colour, or the Apple SF Symbol of the same name
// (filled variant where the symbol set has one).
Image {
    id: icon
    property string name
    property color color: Theme.text
    property int size: 18
    property bool filled: false

    width: size
    height: size
    source: name ? App.icon(name, color.toString(), filled) : ""
    sourceSize: Qt.size(Math.ceil(size * Screen.devicePixelRatio), Math.ceil(size * Screen.devicePixelRatio))
    smooth: true
    asynchronous: false
    cache: true
}
