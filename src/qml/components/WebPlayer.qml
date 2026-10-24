import QtQuick
import QtQuick.Window
import QtWebEngine
import ScanCore

// Hosts a provider's player page inside a small wrapper document
// (PlayerPage). The wrapper polls the player's postMessage API (getTime /
// seek) so the app can resume, auto-advance and skip intros. Pop-ups and
// navigation away from the player are blocked; touch screens get the tap
// shield.
Item {
    id: root
    property string url
    signal time(real seconds, real total, string status)
    signal interacted() // phone only; the desktop page watches the mouse itself
    signal key(var press) // WebPlayerSystem only; shortcuts reach the page here
    // This Qt WebEngine cannot decode H.264, which every episode uses. Qt's
    // own packages are built without it; distribution and Flathub ones have it.
    signal unsupported()
    signal toggleFullScreen()
    property bool reportedUnsupported: false

    function seek(seconds) {
        web.runJavaScript("window.scSeek && window.scSeek(" + Number(seconds) + ")")
    }

    function host(address) {
        return String(address).replace(/^[a-z]+:\/\//i, "").replace(/[\/?#].*$/, "").toLowerCase()
    }

    // Before the first url: shares cookies and the user agent.
    Component.onCompleted: App.prepareWeb()

    onUrlChanged: {
        reportedUnsupported = false
        if (url) web.loadHtml(PlayerPage.wrapper(url, Theme.touch, false), "https://reanime.to/")
    }

    WebEngineView {
        id: web
        anchors.fill: parent
        backgroundColor: "black"
        settings.playbackRequiresUserGesture: false
        settings.fullScreenSupportEnabled: true
        settings.javascriptCanOpenWindows: false
        settings.focusOnNavigationEnabled: true

        onNewWindowRequested: (request) => { /* block pop-up ads */ }
        onNavigationRequested: (request) => {
            // Keep the top frame on the wrapper, and the player's frame on its
            // site: a link clicked or form sent there that leaves it is an ad.
            const target = request.url.toString()
            if (request.isMainFrame && target !== "about:blank" && !target.startsWith("data:") && !target.startsWith("https://reanime.to/"))
                request.reject()
            else if (!request.isMainFrame && root.host(target) !== root.host(root.url)
                     && (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation
                         || request.navigationType === WebEngineNavigationRequest.FormSubmittedNavigation))
                request.reject()
        }
        onFullScreenRequested: (request) => {
            request.accept()
            if (request.toggleOn) Window.window.showFullScreen()
            else Window.window.showNormal()
        }
    }

    // Leaving window full screen some other way (Esc, F11, the player's own
    // button) must also take the page out of it. Otherwise the page still
    // thinks it is full screen and its button only ever asks to exit.
    Connections {
        target: root.Window.window
        function onVisibilityChanged(visibility) {
            if (visibility !== Window.FullScreen && web.isFullScreen) web.fullScreenCancelled()
            const isFs = visibility === Window.FullScreen
            web.runJavaScript("window.__sc.isFs=" + isFs + ";var f=document.getElementById('f');if(f&&f.contentWindow){try{f.contentWindow.postMessage({zenFullscreenState:" + isFs + "},'*')}catch(_){}}")
        }
    }

    Timer {
        interval: 500
        running: root.visible
        repeat: true
        onTriggered: web.runJavaScript("JSON.stringify(window.__sc||{})", (result) => {
            try {
                const s = JSON.parse(result)
                if (s.fs) {
                    web.runJavaScript("window.__sc.fs=false")
                    root.toggleFullScreen()
                }
                if (s.h264 === false && !root.reportedUnsupported) {
                    root.reportedUnsupported = true
                    root.unsupported()
                }
                if (s.d > 0) root.time(s.t, s.d, s.s || "")
            } catch (e) {}
        })
    }
}
