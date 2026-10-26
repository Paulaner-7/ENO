import QtQuick
import Scancore

// Shared-element transition: a cover flies from the card that was clicked
// to its place on the details page. The page calls Router.shell.land(item)
// once its cover is laid out; if it never does, the proxy fades away.
Item {
    id: layer
    property rect from: Qt.rect(0, 0, 0, 0)
    property Item target: null
    property real progress: 0
    property bool active: false
    enabled: false

    function launch(fromItem, source) {
        if (Motion.reduced || !fromItem) return
        const r = fromItem.mapToItem(layer, 0, 0, fromItem.width, fromItem.height)
        from = Qt.rect(r.x, r.y, r.width, r.height)
        proxy.source = source
        target = null
        progress = 0
        proxy.opacity = 1
        active = true
        giveUp.restart()
    }

    function land(item) {
        if (!active || !item) return
        giveUp.stop()
        target = item
        target.opacity = 0
        fly.restart()
    }

    function lerp(a, b, t) { return a + (b - a) * t }

    readonly property rect to: {
        // Re-evaluated every frame while progress animates, so the target
        // is tracked even while the page itself is still sliding in.
        progress
        if (!target) return from
        const r = target.mapToItem(layer, 0, 0, target.width, target.height)
        return Qt.rect(r.x, r.y, r.width, r.height)
    }

    Cover {
        id: proxy
        visible: layer.active
        x: layer.lerp(layer.from.x, layer.to.x, layer.progress)
        y: layer.lerp(layer.from.y, layer.to.y, layer.progress)
        width: layer.lerp(layer.from.width, layer.to.width, layer.progress)
        height: layer.lerp(layer.from.height, layer.to.height, layer.progress)
        radius: layer.lerp(Theme.radiusMd, Theme.radiusLg, layer.progress)
    }

    SequentialAnimation {
        id: fly
        NumberAnimation {
            target: layer
            property: "progress"
            from: 0
            to: 1
            duration: Motion.hero
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Motion.emphasized
        }
        ScriptAction {
            script: {
                if (layer.target) layer.target.opacity = 1
                layer.active = false
                layer.target = null
            }
        }
    }

    Timer {
        id: giveUp
        interval: 700
        onTriggered: fade.restart()
    }
    SequentialAnimation {
        id: fade
        NumberAnimation { target: proxy; property: "opacity"; to: 0; duration: Motion.normal }
        ScriptAction { script: layer.active = false }
    }
}
