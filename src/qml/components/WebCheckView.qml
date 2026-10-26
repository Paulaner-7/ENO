import QtQuick
import QtWebEngine
import Scancore

// A plain browser view for passing a site's browser check. Cookies it
// receives are shared with SCANCORE's own requests (see WebBridge).
WebEngineView {
    id: view
    property string startUrl
    backgroundColor: "white"
    // The user agent the extension met the check with, so the clearance
    // works for it; SCANCORE's again once the window closes.
    onStartUrlChanged: {
        profile.httpUserAgent = App.siteAgent(startUrl)
        url = startUrl
    }
    Component.onDestruction: profile.httpUserAgent = App.defaultAgent()
    settings.javascriptCanOpenWindows: false
    onNewWindowRequested: (request) => { /* stay in this view */ }
    // Before startUrl is set: shares cookies and the user agent.
    Component.onCompleted: App.prepareWeb()
}
