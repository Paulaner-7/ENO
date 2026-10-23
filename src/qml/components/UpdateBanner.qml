import QtQuick
import ScanCore

// Floating card that appears when a new version is downloaded (or can be).
Item {
    id: banner
    readonly property bool shown: Updater.state === "ready" || (Updater.state === "available" && !Updater.canInstall)
    property bool dismissed: false

    width: Math.min(parent.width - 32, 420)
    height: content.implicitHeight + 28
    anchors.horizontalCenter: parent.horizontalCenter
    y: shown && !dismissed ? parent.height - height - 24 - (Theme.compact ? 70 : 0) : parent.height + 20
    opacity: shown && !dismissed ? 1 : 0
    visible: opacity > 0
    Behavior on y { NumberAnimation { duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
    Behavior on opacity { NumberAnimation { duration: Motion.normal } }

    LiquidGlass {
        anchors.fill: parent
        zTier: 1
        radius: Theme.radiusLg
        interactive: true
        trackPointer: true
    }

    NativeGlassButton {
        anchors.fill: parent
        visible: !Theme.eink && isSupported
        glass: false
        zTier: 1
        layout: "toast"
        passthrough: true
        iconName: "download"
        iconColor: Theme.accentInk
        accentColor: Theme.accent
        text: qsTr("Upstream SCANCORE ") + Updater.version + qsTr(" is out")
        badgeText: qsTr("Ask Antigravity to sync ENO with this update.")
        textColor: Theme.text
        badgeTextColor: Theme.textMuted
        fontSize: Theme.textMd
        rightInset: actions.width + 26
    }

    Row {
        id: content
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 32
        spacing: 14
        Rectangle {
            width: 40; height: 40; radius: 12
            color: Theme.accent
            anchors.verticalCenter: parent.verticalCenter
            Icon { anchors.centerIn: parent; name: "download"; size: 18; color: Theme.accentInk }
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 54 - actions.width - 14
            spacing: 2
            Text {
                width: parent.width
                text: qsTr("Upstream SCANCORE ") + Updater.version + qsTr(" is out")
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: qsTr("Ask Antigravity to sync ENO with this update.")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                elide: Text.ElideRight
            }
        }
        Row {
            id: actions
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            ScButton {
                compact: true
                variant: "primary"
                text: qsTr("Release notes")
                inGlassBar: true
                zTier: 1
                onClicked: App.openUrl(Updater.pageUrl)
            }
            IconButton { iconName: "x"; diameter: 30; iconSize: 14; inGlassBar: true; zTier: 1; tip: qsTr("Later"); onClicked: banner.dismissed = true }
        }
    }
}
