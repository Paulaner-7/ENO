import QtQuick
import Scancore

// Loading, error and empty states for a task-backed view.
Item {
    id: root
    property var task: null
    property bool loading: task ? task.loading : false
    property string error: task && !task.loading && task.error !== "Cancelled" ? task.error : ""
    property string verifyUrl: task && !task.loading ? (task.verifyUrl || "") : ""
    property bool empty: false
    property string emptyTitle: qsTr("Nothing here yet")
    property string emptyText: ""
    property string emptyIcon: "sparkles"
    property bool compact: false
    signal retry()

    implicitHeight: compact ? 120 : 320

    // Once the reader passed this site's check, try again by itself.
    Connections {
        target: App
        enabled: root.verifyUrl.length > 0
        function onSiteChecked(url) {
            const host = (u) => u.replace(/^https?:\/\//, "").replace(/[\/:].*$/, "")
            if (host(url) === host(root.verifyUrl)) root.retry()
        }
    }
    visible: loading || error.length > 0 || empty

    Spinner {
        anchors.centerIn: parent
        visible: root.loading
        size: 26
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 420)
        spacing: 12
        visible: !root.loading && (root.error.length > 0 || root.empty)
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }

        Rectangle {
            visible: !root.compact
            anchors.horizontalCenter: parent.horizontalCenter
            width: 52
            height: 52
            radius: 26
            color: Theme.surface2
            Icon {
                anchors.centerIn: parent
                name: root.error.length > 0 ? "wifi-off" : root.emptyIcon
                size: 22
                color: Theme.textMuted
            }
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.error.length > 0 ? qsTr("Something went wrong") : root.emptyTitle
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: root.compact ? Theme.textMd : Theme.textLg
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: text.length > 0
            text: root.error.length > 0 ? root.error : root.emptyText
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            wrapMode: Text.Wrap
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            visible: root.error.length > 0
            ScButton {
                visible: root.verifyUrl.length > 0
                variant: "primary"
                text: qsTr("Open browser check")
                iconName: "globe"
                compact: true
                onClicked: Router.openSiteCheck(root.verifyUrl)
            }
            ScButton {
                text: qsTr("Try again")
                iconName: "refresh-cw"
                compact: true
                onClicked: root.retry()
            }
        }
    }
}
