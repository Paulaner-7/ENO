import QtQuick
import QtQuick.Shapes
import Scancore

// Indeterminate progress arc.
Item {
    id: spinner
    property int size: 22
    property color color: Theme.textMuted
    property real lineWidth: Math.max(2, size / 10)

    width: size
    height: size

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 900
            loops: Animation.Infinite
            // E-ink: a still arc (every frame would flash the screen).
            running: spinner.visible && !Theme.eink
        }
        ShapePath {
            strokeColor: spinner.color
            strokeWidth: spinner.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: spinner.size / 2
                centerY: spinner.size / 2
                radiusX: spinner.size / 2 - spinner.lineWidth
                radiusY: radiusX
                startAngle: 0
                sweepAngle: 270
            }
        }
    }
}
