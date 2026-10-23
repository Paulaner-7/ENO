import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Per-section navigation stack. Pages slide and fade with a slight depth
// shift; the page underneath recedes rather than vanishing.
StackView {
    id: stack
    property string initial

    Component.onCompleted: if (initial) push(Qt.resolvedUrl("../" + initial), {}, StackView.Immediate)

    pushEnter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 36; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
            NumberAnimation { property: "scale"; from: 0.985; to: 1; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }
    pushExit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 0; to: -24; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
            NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }
    popEnter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: -24; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
            NumberAnimation { property: "scale"; from: 0.985; to: 1; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }
    popExit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 0; to: 36; duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.accelerate }
            NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.accelerate }
        }
    }
    replaceEnter: pushEnter
    replaceExit: pushExit
}
