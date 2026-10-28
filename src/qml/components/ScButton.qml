import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Text button. variant: "primary" | "secondary" | "ghost" | "plain" | "danger".
// iconAfter puts the icon after the text (an arrow on "Continue").
AbstractButton {
    id: control
    property string variant: "secondary"
    property string iconName
    property bool loading: false
    property bool compact: false
    property bool iconAfter: false
    property bool inGlassBar: false
    property bool glassBezel: !inGlassBar
    property int zTier: 0

    readonly property color fill: variant === "primary" ? Theme.accent
        : variant === "secondary" ? Theme.surface2
        : variant === "danger" ? Theme.alpha(Theme.danger, 0.14) : "transparent"
    readonly property color ink: variant === "primary" ? Theme.accentInk
        : variant === "danger" ? Theme.danger : variant === "plain" ? Theme.textMuted : Theme.text

    implicitHeight: compact ? 30 : 38
    implicitWidth: Math.max(implicitHeight, row.implicitWidth + (compact ? 22 : 30))
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    font.family: Theme.fontSans
    font.pixelSize: compact ? Theme.textSm : Theme.textMd
    font.weight: Font.DemiBold

    anchors.verticalCenter: (parent && String(parent).indexOf("QQuickRow") !== -1) ? parent.verticalCenter : undefined
    transform: Translate { y: (control.parent && String(control.parent).indexOf("QQuickFlow") !== -1) ? Math.max(0, (38 - control.height) / 2) : 0 }

    NativeGlassButton {
        id: nativeBtn
        anchors.fill: parent
        visible: (control.glassBezel || control.inGlassBar) && !Theme.eink && !control.loading && isSupported
        glass: control.glassBezel
        zTier: control.zTier
        iconName: control.iconName
        iconSize: control.compact ? 14 : 16
        iconColor: control.ink
        iconAfter: control.iconAfter
        text: control.text
        textColor: control.ink
        fontSize: control.compact ? Theme.textSm : Theme.textMd
        bold: true
        cornerRadius: height / 2
        style: control.variant === "primary" ? "regular" : "clear"
        interactive: true
        fillColor: control.fill
        tintColor: control.variant === "primary" ? Theme.alpha(Theme.accent, 0.36)
            : control.variant === "danger" ? Theme.alpha(Theme.danger, 0.25)
            : Qt.rgba(0, 0, 0, 0)
        borderColor: control.variant === "ghost" || control.variant === "secondary" ? Theme.border : Qt.rgba(0, 0, 0, 0)
        onClicked: control.clicked()
    }

    background: Item {
        visible: !nativeBtn.visible
        LiquidGlass {
            anchors.fill: parent
            visible: control.glassBezel && !Theme.eink
            nativeEnabled: false
            radius: height / 2
            glassStyle: control.variant === "primary" ? "regular" : "clear"
            interactive: true
            externalHovered: control.hovered
            externalPressed: control.pressed
            trackPointer: false
            tintColor: control.variant === "primary"
                ? Theme.alpha(Theme.accent, 0.36)
                : control.variant === "danger"
                ? Theme.alpha(Theme.danger, 0.25)
                : Theme.alpha(control.fill, control.hovered ? (Theme.dark ? 0.68 : 0.78) : (Theme.dark ? 0.55 : 0.65))
        }
        Rectangle {
            anchors.fill: parent
            visible: !control.glassBezel || Theme.eink
            radius: height / 2
            color: control.fill
            border.width: control.variant === "ghost" || (control.variant === "secondary" && control.hovered) ? 1 : 0
            border.color: control.hovered ? Theme.borderStrong : Theme.border
            scale: control.pressed ? 0.96 : control.hovered ? 1.02 : 1
            Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring } }
            Behavior on border.color { ColorAnimation { duration: Motion.fast } }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: control.variant === "primary" ? Theme.accentInk : Theme.text
                opacity: control.pressed ? 0.12 : control.hovered ? 0.06 : 0
                Behavior on opacity { NumberAnimation { duration: Motion.fast } }
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: Theme.pill(height)
                color: "transparent"
                border.width: 2
                border.color: Theme.alpha(Theme.accent, 0.6)
                visible: control.visualFocus
            }
        }
    }

    contentItem: Item {
        visible: !nativeBtn.visible
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.iconAfter ? 6 : 8
            layoutDirection: control.iconAfter ? Qt.RightToLeft : Qt.LeftToRight
            Spinner {
                visible: control.loading
                size: 14
                color: control.ink
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: control.iconName.length > 0 && !control.loading
                name: control.iconName
                size: control.compact ? 14 : 16
                color: control.ink
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                visible: control.text.length > 0
                text: control.text
                font: control.font
                color: control.ink
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
