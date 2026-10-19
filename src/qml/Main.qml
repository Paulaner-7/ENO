import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import ScanCore

ApplicationWindow {
    id: window
    width: 1360
    height: 860
    minimumWidth: 360
    minimumHeight: 560
    visible: true
    title: "KAI"
    // Clear only when the window has an alpha channel to show through.
    color: Customize.translucentWindow ? "transparent" : Theme.base
    font.family: Theme.fontSans
    // The default background and palette come from the platform theme (GTK,
    // KDE), which would tint the app with the system's colours. The custom
    // theme may add a picture here.
    background: WindowBackdrop {
        playing: !window.immersiveOpen && window.visibility !== Window.Minimized && window.visibility !== Window.Hidden
    }
    palette {
        window: Theme.base
        windowText: Theme.text
        base: Theme.surface
        alternateBase: Theme.surface2
        text: Theme.text
        button: Theme.surface2
        buttonText: Theme.text
        brightText: Theme.text
        highlight: Theme.accent
        highlightedText: Theme.accentInk
        placeholderText: Theme.textFaint
        toolTipBase: Theme.surface3
        toolTipText: Theme.text
        link: Theme.accent
        mid: Theme.surface3
        dark: Theme.base
        light: Theme.surface3
    }

    readonly property bool immersiveOpen: immersive.depth > 0

    // Settings → Interface: how navigation sits on wide windows. Phones
    // always use the bottom tabs.
    readonly property string navStyle: Theme.compact ? "tabs"
        : ["sidebar", "floating", "rail", "dock"].indexOf(App.settings.navStyle) >= 0 ? App.settings.navStyle : "sidebar"
    readonly property string navSide: navStyle === "dock" ? (App.settings.dockSide === "top" ? "top" : "bottom")
        : App.settings.navSide === "right" ? "right" : "left"
    readonly property bool sidebarShown: navStyle === "sidebar" || navStyle === "floating" || navStyle === "rail"
    readonly property int navInset: navStyle === "floating" ? 10 : 0

    // A .sct theme dropped on the window, or opened from Settings.
    property var themeTask: null
    property url pendingTheme
    function importTheme(url) {
        themeTask = Customize.importTheme(url)
    }
    Connections {
        target: window.themeTask
        function onFinished(theme) { Router.applyTheme(theme) }
        function onFailed(error) { shell.toast(error, "error") }
    }


    Component.onCompleted: {
        Theme.compact = Qt.binding(() => window.width < 720)
        Router.shell = shell
        const start = App.startAction
        if (start.size) {
            window.width = start.size[0]
            window.height = start.size[1]
        }
        if (start.screenshot) screenshotTimer.start()
        // The page chosen in Settings → Interface → Start.
        const first = start.section || App.settings.startSection
        if (first && first !== "home") shell.goSection(first)
        if (start.siteCheck) Qt.callLater(() => Router.openSiteCheck(start.siteCheck))
        if (start.toasts) for (const t of start.toasts) shell.toast(t.message, t.kind)
        if (start.palette) Qt.callLater(() => commandPalette.open())
        if (start.shortcuts) Qt.callLater(() => shortcutsSheet.open())
        if (start.push) Qt.callLater(() => shell.push(start.push.path, start.push.props || {}))
        if (start.installNovelPack) {
            const task = Extensions.installNovelPack()
            task.finished.connect((r) => console.log("novel pack installed:", JSON.stringify(r)))
            task.failed.connect((e) => console.log("novel pack failed:", e))
        }
        if (start.open) Qt.callLater(() => {
            if (start.autoResume) shell.resume(start.open)
            else Router.openSeries(start.open, null)
        })
    }

    Connections {
        target: Library
        function onUpdatesFound(count, titles) {
            shell.toast(count + qsTr(" new chapter") + (count === 1 ? "" : "s") + qsTr(" in your library"), "success")
        }
    }
    Connections {
        target: Downloads
        function onFinished(title) { shell.toast(qsTr("Downloaded ") + title, "success") }
    }
    // A file or link given on the command line or by a second launch.
    Connections {
        target: App
        function onOpenRequested(target) {
            const lower = String(target).toLowerCase()
            if (lower.endsWith(".sct")) {
                window.pendingTheme = target
                themeConfirm.open()
            } else if (/\.(cbz|zip|cbr|rar|cb7|7z|cbt|pdf|epub)$/.test(lower)) {
                Router.openFile(target)
            }
        }
    }

    // On Android a Back that nothing else took can arrive as a close request.
    onClosing: (close) => {
        if (App.platform !== "android") {
            if (App.settings.closeToTray === true) {
                close.accepted = false
                window.hide()
            }
            return
        }
        close.accepted = false
        shell.systemBack()
    }

    // ------------------------------------------------------------------ shell

    QtObject {
        id: shell
        property string section: "home"
        property var visited: ({ home: true })

        function stackFor(name) {
            for (let i = 0; i < sectionRepeater.count; ++i) {
                const loader = sectionRepeater.itemAt(i)
                if (loader && loader.key === name) return loader.item
            }
            return null
        }

        function goSection(name) {
            if (immersive.depth > 0) immersive.clear()
            if (name === section) {
                const stack = stackFor(name)
                if (stack && stack.depth > 1) stack.pop(null)
                return
            }
            // A new object so bindings on `visited` see the change.
            const v = Object.assign({}, visited)
            v[name] = true
            visited = v
            section = name
            Router.section = name
        }

        function push(path, props, fromCover, coverSource) {
            if (immersive.depth > 0) immersive.clear()
            const stack = stackFor(section)
            if (!stack) return
            if (fromCover && coverSource) hero.launch(fromCover, coverSource)
            stack.push(Qt.resolvedUrl(path), props)
        }

        function openImmersive(path, props) {
            immersive.push(Qt.resolvedUrl(path), props)
        }

        // Closes the reader or player, or pops the current page. Returns
        // false when there is nothing to go back to.
        function back() {
            if (immersive.depth > 0) {
                // pop() never removes a stack's last item, and a reader is
                // always alone in this one.
                immersive.popCurrentItem()
                return true
            }
            const stack = stackFor(section)
            if (stack && stack.depth > 1) {
                stack.pop()
                return true
            }
            return false
        }

        // Back from the system (Android Back, Alt+Left): go back, and on
        // Android then return to Home and finally send the app to the
        // background, as Back does on an app's first screen. Android does not
        // close an app's last window.
        function systemBack() {
            if (back() || App.platform !== "android") return
            if (section !== "home") goSection("home")
            else App.moveToBackground()
        }

        function toast(message, kind) { toasts.show(message, kind) }

        function land(target) { hero.land(target) }

        function resume(entry) {
            const props = { url: entry.sourceUrl, summary: entry, autoResume: true }
            if (entry.media === "anime") {
                const extra = entry.extra || {}
                push("pages/AnimeDetails.qml", { anilistId: extra.anilistId || 0, summary: entry, autoResume: true })
            } else if (entry.media === "novel") {
                push("pages/NovelDetails.qml", props)
            } else {
                push("pages/ComicDetails.qml", props)
            }
        }

        function openPalette() { commandPalette.open() }

        function openShortcuts() { shortcutsSheet.open() }

        function showWelcome() { onboarding.active = true }
    }

    // A focus scope that holds focus whenever no reader is open, so Escape
    // from any page (or from nowhere in particular) reaches it.
    FocusScope {
        id: shellView
        anchors.fill: parent
        focus: true
        // The parent's safe area, so these margins do not feed back into it.
        anchors.topMargin: parent.SafeArea.margins.top
        anchors.bottomMargin: parent.SafeArea.margins.bottom
        enabled: !window.immersiveOpen
        opacity: window.immersiveOpen ? 0 : 1
        scale: window.immersiveOpen ? 0.985 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
        Behavior on scale { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
        // Disabling drops focus; take it back when the reader closes.
        onEnabledChanged: if (enabled) forceActiveFocus()
        Keys.onEscapePressed: (event) => event.accepted = shell.back()

        Shadow {
            target: sidebar
            visible: sidebar.visible && sidebar.floating
            lift: 0.5
        }
        Sidebar {
            id: sidebar
            visible: window.sidebarShown
            style: window.navStyle
            side: window.navSide
            width: visible ? implicitWidth : 0
            x: side === "right" ? parent.width - width - window.navInset : window.navInset
            y: window.navInset
            height: parent.height - 2 * window.navInset
            current: shell.section
            onNavigate: (name) => shell.goSection(name)
            onOpenPalette: commandPalette.open()
        }

        Item {
            id: content
            anchors.left: window.sidebarShown && window.navSide === "left" ? sidebar.right : parent.left
            anchors.right: window.sidebarShown && window.navSide === "right" ? sidebar.left : parent.right
            anchors.top: dock.visible && window.navSide === "top" ? dock.bottom : parent.top
            anchors.bottom: Theme.compact ? tabBar.top : dock.visible && window.navSide === "bottom" ? dock.top : parent.bottom
            anchors.topMargin: dock.visible && window.navSide === "top" ? 6 : 0
            anchors.bottomMargin: dock.visible && window.navSide === "bottom" ? 6 : 0
            clip: true

            Repeater {
                id: sectionRepeater
                model: ["home", "comics", "novels", "anime", "library", "history", "activity", "downloads", "settings"]
                delegate: Loader {
                    id: sectionLoader
                    required property string modelData
                    readonly property string key: modelData
                    readonly property bool current: shell.section === key
                    anchors.fill: parent
                    active: shell.visited[key] === true
                    visible: opacity > 0.001
                    opacity: current ? 1 : 0
                    z: current ? 1 : 0
                    transform: Translate { y: sectionLoader.current ? 0 : 14; Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate } } }
                    Behavior on opacity { NumberAnimation { duration: sectionLoader.current ? Motion.slow : Motion.fast; easing.type: Easing.OutCubic } }

                    sourceComponent: PageStack {
                        initial: {
                            switch (sectionLoader.key) {
                            case "home": return "pages/HomePage.qml"
                            case "comics": return "pages/ComicsPage.qml"
                            case "novels": return "pages/NovelsPage.qml"
                            case "anime": return "pages/AnimePage.qml"
                            case "library": return "pages/LibraryPage.qml"
                            case "history": return "pages/HistoryPage.qml"
                            case "activity": return "pages/ActivityPage.qml"
                            case "downloads": return "pages/DownloadsPage.qml"
                            default: return "pages/SettingsPage.qml"
                            }
                        }
                    }
                }
            }
        }
    }

    // Wide windows with the dock style: a floating bar along the top or bottom.
    Shadow {
        parent: shellView
        target: dock
        visible: dock.visible
        lift: 0.8
        z: 2
    }
    Dock {
        id: dock
        parent: shellView
        visible: window.navStyle === "dock"
        side: window.navSide
        z: 3
        anchors.horizontalCenter: parent.horizontalCenter
        y: side === "top" ? 12 : parent.height - height - 12
        width: Math.min(implicitWidth, parent.width - 24)
        height: implicitHeight
        current: shell.section
        onNavigate: (name) => shell.goSection(name)
        onOpenPalette: commandPalette.open()
    }

    // Phone layout: bottom tabs instead of the sidebar.
    TabBar {
        id: tabBar
        parent: shellView
        visible: Theme.compact
        height: Theme.compact ? 62 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        current: shell.section
        onNavigate: (name) => shell.goSection(name)
        onOpenPalette: commandPalette.open()
    }

    // Readers and the player cover the whole window.
    StackView {
        id: immersive
        // Readers that set edgeToEdge draw under the system bars and pad
        // their own controls; everything else stays inside the safe area.
        readonly property bool edgeToEdge: currentItem !== null && currentItem.edgeToEdge === true
        anchors.fill: parent
        // The parent's safe area, so these margins do not feed back into it.
        anchors.topMargin: edgeToEdge ? 0 : parent.SafeArea.margins.top
        anchors.bottomMargin: edgeToEdge ? 0 : parent.SafeArea.margins.bottom
        visible: depth > 0 || busy
        z: 10
        // Readers handle their own keys, so they need focus as soon as they open.
        onCurrentItemChanged: if (currentItem) currentItem.forceActiveFocus()
        pushEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 1.03; to: 1; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
            }
        }
        pushExit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }
        popEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal } }
        popExit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: Motion.normal }
                NumberAnimation { property: "scale"; to: 1.02; duration: Motion.normal }
            }
        }
        replaceEnter: pushEnter
        replaceExit: pushExit
    }

    HeroLayer {
        id: hero
        anchors.fill: parent
        z: 20
    }

    // First-run welcome; Settings can open it again.
    Loader {
        id: onboarding
        anchors.fill: parent
        z: 50
        active: App.settings.onboarded !== true
        sourceComponent: Onboarding {
            onFinished: onboarding.active = false
        }
    }

    CommandPalette {
        id: commandPalette
        z: 30
    }

    ShortcutsSheet {
        id: shortcutsSheet
        z: 30
    }

    UpdateBanner {
        z: 35
    }

    // Drop a .sct theme on the window to use it.
    DropArea {
        anchors.fill: parent
        z: 60
        onEntered: (drag) => drag.accepted = drag.hasUrls && drag.urls.some(u => String(u).toLowerCase().endsWith(".sct"))
        onDropped: (drop) => {
            const url = drop.urls.find(u => String(u).toLowerCase().endsWith(".sct"))
            if (!url) return
            window.pendingTheme = url
            themeConfirm.open()
        }
    }
    ConfirmDialog {
        id: themeConfirm
        heading: qsTr("Use this theme?")
        message: decodeURIComponent(String(window.pendingTheme).replace(/^.*\//, ""))
            + qsTr(" replaces your custom theme and switches SCANCORE to it.")
        confirmText: qsTr("Use theme")
        onConfirmed: window.importTheme(window.pendingTheme)
    }
    Toasts {
        id: toasts
        z: 40
    }

    // Development hook: SCANCORE_START {"screenshot": "/path.png", "delay": ms}
    Timer {
        id: screenshotTimer
        interval: App.startAction.delay || 8000
        onTriggered: {
            App.saveWindowImage(window, App.startAction.screenshot)
            Qt.quit()
        }
    }

    // ------------------------------------------------------------------ keys

    // The keys come from Settings → Keyboard (see Keymap).
    Shortcut { sequences: Keymap.sequences("palette"); onActivated: commandPalette.open() }
    Shortcut { sequences: Keymap.sequences("shortcuts"); enabled: App.platform !== "android"; onActivated: shortcutsSheet.open() }
    // StandardKey.Back is also Android's Back key.
    Shortcut { sequences: Keymap.sequences("back").concat([StandardKey.Back]); onActivated: shell.systemBack() }
    // The back button on mice.
    TapHandler {
        acceptedButtons: Qt.BackButton
        onTapped: shell.back()
    }
    Shortcut {
        sequences: Keymap.sequences("sidebar")
        enabled: window.navStyle === "sidebar" || window.navStyle === "floating"
        onActivated: App.setSetting("sidebarCollapsed", !(App.settings.sidebarCollapsed === true))
    }
    Shortcut { sequences: Keymap.sequences("home"); onActivated: shell.goSection("home") }
    Shortcut { sequences: Keymap.sequences("comics"); onActivated: shell.goSection("comics") }
    Shortcut { sequences: Keymap.sequences("novels"); onActivated: shell.goSection("novels") }
    Shortcut { sequences: Keymap.sequences("anime"); onActivated: shell.goSection("anime") }
    Shortcut { sequences: Keymap.sequences("library"); onActivated: shell.goSection("library") }
    Shortcut { sequences: Keymap.sequences("history"); onActivated: shell.goSection("history") }
    Shortcut { sequences: Keymap.sequences("activity"); onActivated: shell.goSection("activity") }
    Shortcut { sequences: Keymap.sequences("downloads"); onActivated: shell.goSection("downloads") }
    Shortcut { sequences: Keymap.sequences("settings"); onActivated: shell.goSection("settings") }
    Shortcut {
        sequences: Keymap.sequences("fullscreen")
        onActivated: window.visibility === Window.FullScreen ? window.showNormal() : window.showFullScreen()
    }
}
