import QtQuick
import ScanCore

// Puts a page's PageSections (children of `column`) in the user's order and
// runs its edit mode. The sections stay where the page declares them; they
// are only restacked, so nothing is rebuilt when the order changes.
QtObject {
    id: arranger
    // The page's key in PageLayout ("home", "comics", …).
    property string page
    property Item column
    property bool editing: false
    // Sections can be removed and added (Home).
    property bool removable: false

    // The default order of the page's section keys. Sections made later
    // (by a Repeater) whose keys are not listed come after these.
    property var defaults: []
    readonly property var declared: {
        const out = Array.from(defaults)
        for (const s of present) if (out.indexOf(s) < 0) out.push(s)
        return out
    }
    property var present: []
    // The key of the section being dragged, and where it would land.
    property string dragging: ""
    property int dropIndex: -1
    property real dragHeight: 0

    function sections() {
        return column ? Array.from(column.children).filter(c => c.isPageSection === true) : []
    }

    function apply() {
        const list = sections()
        if (list.length === 0) return
        const keys = list.map(s => s.key)
        if (keys.some(k => present.indexOf(k) < 0) || present.length !== keys.length)
            present = keys
        const order = PageLayout.order(page, declared).filter(k => keys.indexOf(k) >= 0)
        if (order.join("|") === keys.join("|")) return
        let previous = null
        for (const key of order) {
            const section = list.find(s => s.key === key)
            if (!section) continue
            if (previous) {
                App.stackAfter(section, previous)
            } else {
                const first = sections()[0]
                if (first !== section) App.stackBefore(section, first)
            }
            previous = section
        }
    }

    // One place up or down among the sections on the page now (shelves a
    // catalogue does not have are skipped).
    function move(key, delta) {
        const order = PageLayout.order(page, declared)
        const here = sections().map(s => s.key)
        const shown = order.filter(k => here.indexOf(k) >= 0)
        const to = shown.indexOf(key) + delta
        if (to < 0 || to >= shown.length) return
        PageLayout.moveTo(page, declared, key, order.indexOf(shown[to]))
    }
    function setHidden(key, hidden) { PageLayout.setHidden(page, key, hidden) }
    function reset() { PageLayout.reset(page) }

    // Where a section dragged to `centerY` (in the column) lands.
    function indexAt(key, centerY) {
        const others = sections().filter(s => s.key !== key && s.visible).sort((a, b) => a.y - b.y)
        let index = 0
        for (const s of others)
            if (centerY > s.y + s.height / 2) index = PageLayout.order(page, declared).indexOf(s.key) + 1
        const from = PageLayout.order(page, declared).indexOf(key)
        return index > from ? index - 1 : index
    }
    function drop(key) {
        if (dropIndex >= 0) PageLayout.moveTo(page, declared, key, dropIndex)
        dragging = ""
        dropIndex = -1
    }

    property Connections layoutWatch: Connections {
        target: PageLayout
        function onRevisionChanged() { arranger.apply() }
    }
    // Sections a Repeater adds (catalogue shelves) take their place too.
    property Connections childrenWatch: Connections {
        target: arranger.column
        function onChildrenChanged() { Qt.callLater(arranger.apply) }
    }
    Component.onCompleted: Qt.callLater(apply)
}
