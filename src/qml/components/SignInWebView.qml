import QtQuick
import QtWebEngine
import ScanCore

// A sign-in page inside SCANCORE: reports each address it goes to, so the
// page that hosts it can take what the service hands back (AniList's token).
WebEngineView {
    id: view
    property string startUrl
    signal visited(string address)
    backgroundColor: "white"
    onStartUrlChanged: url = startUrl
    onUrlChanged: view.visited(url.toString())
    settings.javascriptCanOpenWindows: false
    onNewWindowRequested: (request) => { /* stay in this view */ }
    Component.onCompleted: App.prepareWeb()
}
