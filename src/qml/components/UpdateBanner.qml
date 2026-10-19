import QtQuick
import ScanCore

// Floating card that appears when a new version is downloaded (or can be).
Rectangle {
    id: banner
    readonly property bool shown: Updater.state === "ready" || (Updater.state === "available" && !Updater.canInstall)
    property bool dismissed: false

    width: Math.min(parent.width - 32, 420)
    height: content.implicitHeight + 28
    radius: Theme.radiusLg
    color: Theme.surface
    border.width: 1
    border.color: Theme.borderStrong
    anchors.horizontalCenter: parent.horizontalCenter
    y: shown && !dismissed ? parent.height - height - 24 - (Theme.compact ? 70 : 0) : parent.height + 20
    opacity: shown && !dismissed ? 1 : 0
    visible: opacity > 0
    Behavior on y { NumberAnimation { duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
    Behavior on opacity { NumberAnimation { duration: Motion.normal } }

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
                text: qsTr("Ask Antigravity to sync KAI with this update.")
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
                onClicked: App.openUrl(Updater.pageUrl)
            }
            IconButton { iconName: "x"; diameter: 30; iconSize: 14; tip: qsTr("Later"); onClicked: banner.dismissed = true }
        }
    }
}
