import QtQuick
import Scancore

// Full screen while reading (comics and novels). The readers' full screen
// button switches it and is remembered (setting readerFullscreen), so the
// next chapter opens the same way; on a phone it hides the status and
// navigation bars. The window goes back to how it was when the reader
// closes.
Item {
    id: root
    visible: false
    readonly property var win: Window.window
    readonly property bool fullScreen: win !== null && win.visibility === Window.FullScreen
    // The window this put in full screen, and how it was before.
    property var held: null
    property int before: Window.Windowed

    function enter() {
        if (!win || fullScreen) return
        before = win.visibility
        held = win
        win.showFullScreen()
    }
    function leave() {
        const w = held || win
        held = null
        if (!w || w.visibility !== Window.FullScreen) return
        if (before === Window.Maximized) w.showMaximized()
        else w.showNormal()
    }
    function toggle() {
        const on = !fullScreen
        App.setSetting("readerFullscreen", on)
        if (on) enter()
        else leave()
    }

    onWinChanged: if (win && App.settings.readerFullscreen === true) Qt.callLater(root.enter)
    Component.onDestruction: if (held) leave()
}
