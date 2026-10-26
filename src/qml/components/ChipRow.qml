import QtQuick
import Scancore

// A row of chips that spans its parent. Wide windows wrap it onto more
// lines; phones keep it on one line that scrolls sideways, so a long list of
// sources does not push the page down.
Flickable {
    id: root
    maximumFlickVelocity: Motion.flickVelocity
    Glide { flick: root }
    flickDeceleration: Motion.flickDeceleration
    default property alias chips: flow.data
    property real padding: Theme.gutter
    property var hooked: []

    width: parent ? parent.width : 400
    height: flow.height
    // The Flow's own size, not its childrenRect (reading that from a binding
    // re-enters the layout: a binding loop).
    contentWidth: Theme.compact ? flow.implicitWidth + 2 * padding : width
    contentHeight: height
    interactive: Theme.compact
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    // The selected chip stays in view (a list picked by swiping the page).
    function reveal() {
        if (!interactive) return
        for (let i = 0; i < flow.children.length; ++i) {
            const c = flow.children[i]
            if (!c.visible || c.selected !== true) continue
            const left = flow.x + c.x - padding, right = flow.x + c.x + c.width + padding
            if (left < contentX) revealAnim.to = Math.max(0, left)
            else if (right > contentX + width) revealAnim.to = Math.min(contentWidth - width, right - width)
            else return
            revealAnim.restart()
            return
        }
    }
    NumberAnimation { id: revealAnim; target: root; property: "contentX"; duration: Motion.normal; easing.type: Easing.OutCubic }
    Timer { id: revealSoon; interval: 0; onTriggered: root.reveal() }

    Flow {
        id: flow
        // Any chip turning selected (or the chips laid out anew).
        onPositioningComplete: revealSoon.restart()
        onChildrenChanged: {
            for (let i = 0; i < children.length; ++i) {
                const c = children[i]
                if (c.selectedChanged !== undefined && root.hooked.indexOf(c) < 0) {
                    root.hooked.push(c)
                    c.selectedChanged.connect(revealSoon.restart)
                }
            }
        }
        x: root.padding
        width: Theme.compact ? 100000 : root.width - 2 * root.padding
        spacing: 8
    }
}
