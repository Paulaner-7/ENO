import QtQuick
import QtQuick.Window
import QtWebEngine
import ScanCore

// Hosts a provider's player page inside a small wrapper document. The
// wrapper polls the player's postMessage API (getTime / seek) so the app
// can resume, auto-advance and skip intros. Pop-ups and navigation away
// from the player are blocked.
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

    function wrapper(src) {
        const safe = String(src).replace(/"/g, "&quot;").replace(/</g, "&lt;")
        return "<!doctype html><html><head><meta charset='utf-8'>"
            + "<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}iframe{border:0;width:100%;height:100%}</style>"
            + "</head><body><iframe id='f' src=\"" + safe + "\" allow='autoplay; fullscreen; encrypted-media; picture-in-picture' allowfullscreen referrerpolicy='origin'></iframe>"
            + "<script>"
            + "window.__sc={t:0,d:0,s:'',fs:false,h264:!!(window.MediaSource&&MediaSource.isTypeSupported('video/mp4; codecs=\\\"avc1.42E01E\\\"'))};var f=document.getElementById('f');"
            + "addEventListener('message',function(e){if(e.source!==f.contentWindow)return;var x=e.data;"
            + "if(typeof x==='string'){try{x=JSON.parse(x)}catch(_){return}}if(!x)return;"
            + "if(x.currentTime!==undefined){__sc.t=+x.currentTime||0;__sc.d=+x.duration||0}"
            + "if(x.playerStatus)__sc.s=String(x.playerStatus);"
            + "if(x.zenCommand==='toggleFullscreen'){__sc.fs=true;}"
            + "if(x.zenCommand==='getFullscreenState'){try{f.contentWindow.postMessage({zenFullscreenState:!!window.__sc.isFs},'*')}catch(_){}}"
            + "});"
            + "setInterval(function(){try{f.contentWindow.postMessage({command:'getTime'},'*')}catch(_){}},1000);"
            + "window.scSeek=function(v){f.contentWindow.postMessage({command:'seek',value:v},'*')};"
            + "</script></body></html>"
    }

    // Before the first url: shares cookies and the user agent.
    Component.onCompleted: App.prepareWeb()

    onUrlChanged: {
        reportedUnsupported = false
        if (url) web.loadHtml(wrapper(url), "https://reanime.to/")
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
            // Keep the top frame on the wrapper; frames may navigate freely.
            if (request.isMainFrame && request.url.toString() !== "about:blank"
                && !request.url.toString().startsWith("data:") && !request.url.toString().startsWith("https://reanime.to/"))
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
