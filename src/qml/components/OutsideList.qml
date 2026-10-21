import QtQuick
import ScanCore

// A place in a ListView's header for a field that must not be inside the
// list. A ListView gives its first row focus whenever its model is
// replaced, as a live filter does with every letter, so a field inside it
// lost focus after each letter and on Android the keyboard closed, the
// rest of the tap landing on a row behind it. The content is drawn from
// `outside` (the page holding the list) over this placeholder and moves
// with it as the list scrolls.
Item {
    id: slot
    required property Flickable view
    required property Item outside
    default property alias content: holder.data
    // Give it its height: one learned after the list has laid out its
    // header would move the list's content.

    Item {
        id: holder
        parent: slot.outside
        z: 1
        width: slot.width
        height: slot.height
        // The placeholder's place in the list's content, less the scroll.
        x: {
            if (!slot.view) return 0
            let x = 0
            for (let i = slot; i && i !== slot.view.contentItem; i = i.parent) x += i.x
            return x - slot.view.contentX + slot.view.x
        }
        y: {
            if (!slot.view) return 0
            let y = 0
            for (let i = slot; i && i !== slot.view.contentItem; i = i.parent) y += i.y
            return y - slot.view.contentY + slot.view.y
        }
        visible: slot.visible && slot.view !== null && y + height > slot.view.y && y < slot.view.y + slot.view.height
    }
}
