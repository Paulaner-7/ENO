import QtQuick
import QtQuick.Controls.Basic
import Scancore

// A reader, the player or a page in a window of its own (computers): read
// one series while another downloads, the anime on a second screen, or a
// comic and its novel side by side (the desktop's own tiling splits them).
// Router.openWindow makes one; Router.leave(page) closes it again when the
// page is done.
ApplicationWindow {
    id: win
    property string page
    property var props: ({})
    readonly property bool isPageWindow: true

    width: 1100
    height: 820
    minimumWidth: 360
    minimumHeight: 480
    visible: true
    color: Theme.base
    font.family: Theme.fontSans
    title: (props.series && props.series.title ? props.series.title + " · " : "") + "SCANCORE"

    Loader {
        id: content
        anchors.fill: parent
        focus: true
        Component.onCompleted: setSource(Qt.resolvedUrl("../" + win.page), win.props)
        onLoaded: item.forceActiveFocus()
    }
    // Pages that ask to go back close the window instead (Router.leave);
    // Escape too, where the page does not take it.
    Shortcut { sequence: "Esc"; onActivated: win.close() }
    onClosing: Qt.callLater(win.destroy)
}
