import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Round icon button with hover wash, press squish and an optional tooltip.
AbstractButton {
    id: control
    property string iconName
    property int iconSize: 18
    property color iconColor: active ? Theme.text : ((hovered || nativeBtn.hovered) ? Theme.text : Theme.textMuted)
    property bool active: false
    property string tip
    property bool filled: false
    property bool inGlassBar: false
    property int zTier: 0
    property int diameter: 34

    implicitWidth: diameter
    implicitHeight: diameter
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.35

    NativeGlassButton {
        id: nativeBtn
        anchors.fill: parent
        visible: (control.filled || control.inGlassBar) && !Theme.eink && isSupported
        glass: control.filled
        zTier: control.zTier
        active: control.active
        iconName: control.iconName
        iconSize: control.iconSize
        iconColor: control.iconColor
        circular: true
        tip: control.tip
        style: "clear"
        interactive: true
        onClicked: control.clicked()
    }

    background: Item {
        visible: !nativeBtn.visible
        LiquidGlass {
            anchors.fill: parent
            visible: control.filled
            nativeEnabled: false
            radius: control.diameter / 2
            glassStyle: "clear"
            interactive: true
            externalHovered: control.hovered
            externalPressed: control.pressed
            trackPointer: false
            tintColor: Theme.alpha(Theme.surface2, control.hovered ? (Theme.dark ? 0.68 : 0.78) : (Theme.dark ? 0.55 : 0.65))
        }
        Rectangle {
            anchors.fill: parent
            visible: !control.filled
            radius: control.diameter / 2
            color: control.active ? Theme.alpha(Theme.text, 0.10)
                 : control.hovered ? Theme.alpha(Theme.text, 0.07) : "transparent"
            border.width: control.visualFocus ? 2 : 0
            border.color: Theme.accent
            Behavior on color { ColorAnimation { duration: Motion.fast } }
        }
    }

    contentItem: Item {
        visible: !nativeBtn.visible
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.iconColor
            // The current state's symbol: outline when off, filled when on
            // (the native button does the same through its own active state).
            filled: control.active
            scale: control.pressed ? 0.85 : control.hovered ? 1.1 : 1
            Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring } }
        }
    }

    // Made only while a pointer rests on it: the attached ToolTip built a
    // popup for every button, which a chapter list makes by the hundred.
    Loader {
        active: control.tip.length > 0 && (control.hovered || nativeBtn.hovered)
        sourceComponent: ToolTip {
            parent: control
            visible: true
            delay: 600
            text: control.tip
        }
    }
}
