import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Connecting AniList without copying anything: AniList's own sign-in page,
// inside SCANCORE, and the account is connected as soon as AniList hands the
// token over.
Rectangle {
    id: page
    color: Theme.base
    focus: true
    Shortcut { sequence: "Esc"; onActivated: Router.back() }

    property var task: null
    function visited(address) {
        if (task || address.indexOf("access_token=") < 0) return
        task = Trackers.anilistUseToken(address)
        task.finished.connect((r) => {
            Router.toast(qsTr("Connected to %1 as %2").arg("AniList").arg(r.name), "success")
            Router.back()
        })
        task.failed.connect((error) => {
            Router.toast(error, "error")
            page.task = null
        })
    }

    Rectangle {
        id: bar
        width: parent.width
        height: 64
        color: Theme.surface
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        Row {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            IconButton { iconName: "arrow-left"; tip: qsTr("Close"); onClicked: Router.back() }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: qsTr("Connect AniList")
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                }
                Text {
                    text: page.task ? qsTr("Connecting…") : qsTr("Sign in and allow SCANCORE; this page closes by itself.")
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
            }
        }
    }
    Loader {
        anchors.top: bar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        source: "../components/SignInWebView.qml"
        onLoaded: {
            item.visited.connect(page.visited)
            item.startUrl = Trackers.anilistAuthorizeUrl()
        }
    }
}
