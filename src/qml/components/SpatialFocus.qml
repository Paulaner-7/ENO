import QtQuick
import QtQuick.Controls.Basic
import Scancore

// A television's remote (App.tv): the D-pad moves the focus to the nearest
// control in that direction, Select presses it, and a ring shows where the
// focus is. Pages need nothing of their own: every button, cover and tile
// that takes focus with Tab is a stop. The view a stop is in scrolls to
// show it.
Item {
    id: spatial
    // Where to look for stops (the shell; readers keep their own keys).
    property Item scope
    readonly property bool active: App.tv
    readonly property Item current: active && Window.window ? Window.window.activeFocusItem : null

    function isStop(item) {
        if (!item || !item.visible || !item.enabled || item.width <= 0 || item.height <= 0 || item.opacity <= 0.01) return false
        if (item instanceof AbstractButton) return (item.focusPolicy & Qt.TabFocus) !== 0
        return item.activeFocusOnTab === true
    }
    function collect(item, out) {
        if (!item || !item.visible || item.opacity <= 0.01) return
        if (isStop(item)) out.push(item)
        // A stop's own insides are not stops.
        else for (let i = 0; i < item.children.length; ++i) collect(item.children[i], out)
    }
    function rectOf(item) {
        const p = item.mapToItem(null, 0, 0)
        return Qt.rect(p.x, p.y, item.width, item.height)
    }
    // Inside the window (and inside the views clipping it).
    function onScreen(item) {
        const r = rectOf(item)
        const w = Window.window
        return r.x + r.width > 0 && r.y + r.height > 0 && r.x < w.width && r.y < w.height
    }

    // "left", "right", "up" or "down". Returns whether it moved.
    function move(direction) {
        const stops = []
        collect(scope, stops)
        if (stops.length === 0) return false
        const from = current && isStop(current) ? rectOf(current) : null
        if (!from) {
            // Nothing focused yet: the first stop on screen, top left.
            const shown = stops.filter(onScreen).sort((a, b) => rectOf(a).y - rectOf(b).y || rectOf(a).x - rectOf(b).x)
            if (shown.length) focusOn(shown[0])
            return shown.length > 0
        }
        const cx = from.x + from.width / 2, cy = from.y + from.height / 2
        let best = null, bestScore = Infinity
        for (const item of stops) {
            if (item === current) continue
            const r = rectOf(item)
            const x = r.x + r.width / 2, y = r.y + r.height / 2
            let along, across
            switch (direction) {
            case "left": along = from.x - (r.x + r.width); across = Math.abs(y - cy); break
            case "right": along = r.x - (from.x + from.width); across = Math.abs(y - cy); break
            case "up": along = from.y - (r.y + r.height); across = Math.abs(x - cx); break
            default: along = r.y - (from.y + from.height); across = Math.abs(x - cx)
            }
            // Ahead only (a little overlap is fine), the nearest in line first.
            if (along < -Math.min(from.width, from.height) * 0.3) continue
            const score = Math.max(0, along) + across * 2.2
            if (score < bestScore) {
                bestScore = score
                best = item
            }
        }
        if (best) focusOn(best)
        return best !== null
    }

    function focusOn(item) {
        item.forceActiveFocus(Qt.TabFocusReason)
        reveal(item)
    }
    // Scrolls the views around `item` so it shows, with a little room.
    function reveal(item) {
        for (let p = item.parent; p; p = p.parent) {
            if (!(p instanceof Flickable)) continue
            const r = item.mapToItem(p.contentItem, 0, 0)
            const margin = 40
            if (p.contentHeight > p.height) {
                if (r.y - margin < p.contentY) p.contentY = Math.max(p.originY, r.y - margin)
                else if (r.y + item.height + margin > p.contentY + p.height)
                    p.contentY = Math.min(p.originY + p.contentHeight - p.height, r.y + item.height + margin - p.height)
            }
            if (p.contentWidth > p.width) {
                if (r.x - margin < p.contentX) p.contentX = Math.max(p.originX, r.x - margin)
                else if (r.x + item.width + margin > p.contentX + p.width)
                    p.contentX = Math.min(p.originX + p.contentWidth - p.width, r.x + item.width + margin - p.width)
            }
        }
    }
    function activate() {
        if (current instanceof AbstractButton) {
            current.clicked()
            return true
        }
        return false
    }

    // The ring around what has the focus.
    Rectangle {
        id: ring
        parent: Window.window ? Window.window.contentItem : spatial
        z: 1000
        visible: spatial.active && spatial.current !== null && spatial.isStop(spatial.current)
        color: "transparent"
        radius: Theme.radiusMd + 3
        border.width: 3
        border.color: Theme.retro ? Theme.tile : Theme.accentIsMono ? "#ffffff" : Theme.accent
        // Follows the item as it moves or its view scrolls.
        FrameAnimation {
            running: ring.visible
            onTriggered: {
                const r = spatial.rectOf(spatial.current)
                ring.x = r.x - 4
                ring.y = r.y - 4
                ring.width = r.width + 8
                ring.height = r.height + 8
            }
        }
    }
}
