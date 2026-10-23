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
    visibility: App.startAction.screenshot ? Window.Windowed : Window.FullScreen
    title: "ENO"
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
    readonly property Item glassBackdrop: immersiveOpen ? immersive : stage

    // Keep native macOS 27 NSWindow appearance & NSGlassEffectView backdrop in sync.
    readonly property color windowGlassTint: Theme.alpha(Theme.base, Theme.windowOpacity)
    onWindowGlassTintChanged: Customize.syncWindowGlass(window, Theme.dark, windowGlassTint)

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


    // Status bar icons for the screen under them: a reader or the player
    // covers the window and may say (systemBarsDark), else they are dark
    // screens; otherwise the app's theme decides, not the phone's mode.
    readonly property bool darkBars: {
        const page = immersive.currentItem
        if (immersive.depth > 0 && page) return page.systemBarsDark !== undefined ? page.systemBarsDark === true : true
        return Theme.dark
    }
    onDarkBarsChanged: {
        App.setBarIcons(darkBars)
        Customize.syncWindowGlass(window, Theme.dark, windowGlassTint)
    }

    Component.onCompleted: {
        App.setBarIcons(darkBars)
        Customize.syncWindowGlass(window, Theme.dark, windowGlassTint)
        if (!App.startAction.screenshot)
            window.showFullScreen()
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
        // {"notify": {title, body, delay}}: one in the notification area and
        // the system's (after `delay` ms, e.g. once the app is in the
        // background).
        if (start.notify) notifyHook.start()
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
    // A crash last time: the report, to send.
    CrashReportsDialog { id: crashPrompt }
    Timer { running: true; interval: 2500; onTriggered: crashPrompt.showLatest() }

    // Another of the account's apps sent where it is (Continue on…).
    Connections {
        target: Live
        function onHandoffReceived(entry, from) {
            shell.toast(qsTr("From %1: %2, %3").arg(from.name).arg(entry.title).arg(entry.unitTitle), "info",
                        { label: qsTr("Continue"), run: () => shell.resume(entry) })
            // Sent on purpose from the other device: bring this one forward.
            if (!window.visible) window.show()
            window.raise()
            window.requestActivate()
        }
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
            } else if (Extensions.repositoryLink(target).url && !/^https?:/.test(lower)) {
                // A one-tap "add repository" link from a repository's page.
                Router.addRepository(target)
            }
        }
        // An extension's request is waiting for a check only the reader can pass.
        function onBrowserCheckWanted(url) { Router.openSiteCheck(url) }
        // A browser check has been left; pages waiting on it load again.
        function onSiteChecked(url) {
            shell.toast(qsTr("Browser check done for %1").arg(url.replace(/^https?:\/\//, "").replace(/\/.*$/, "")), "success")
        }
    }

    // Extensions that were installing in the background (adding a repository).
    Connections {
        target: Extensions
        function onInstallsFinished(installed, failed, firstError) {
            if (installed > 0)
                shell.toast(installed === 1 ? qsTr("1 extension installed") : qsTr("%1 extensions installed").arg(installed), "success")
            if (failed > 0)
                shell.toast(failed === 1 ? firstError : qsTr("%1 extensions could not be installed: %2").arg(failed).arg(firstError), "error")
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
        // After the first page is up, the others are built over several
        // frames instead of in one go, so a first visit (and the press that
        // led there) never freezes the window; slow phones gain most.
        property bool settled: false

        function stackFor(name) {
            for (let i = 0; i < sectionRepeater.count; ++i) {
                const loader = sectionRepeater.itemAt(i)
                if (loader && loader.key === name) return loader.item
            }
            return null
        }

        function goSection(name) {
            // Not a page: the notification area opens over the current one.
            if (name === "notifications") {
                notificationsPanel.open()
                return
            }
            if (name === "downloads") {
                Router.openSettings("downloads")
                return
            }
            if (name === "history") {
                Router.activityTab = "history"
                goSection("activity")
                return
            }
            if (immersive.depth > 0) immersive.clear()
            // ENO is anime only: Comics and Novels lead to Anime.
            if (!Router.sectionAllowed(name)) name = "anime"
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
            if (App.profiling) App.profilePlace(name)
            Router.section = name
        }

        function push(path, props, fromCover, coverSource) {
            if (immersive.depth > 0) immersive.clear()
            const stack = stackFor(section)
            if (!stack) return
            if (fromCover && coverSource) hero.launch(fromCover, coverSource)
            stack.push(Qt.resolvedUrl(path), props)
            if (App.profiling) App.profilePlace(path)
        }

        function openImmersive(path, props) {
            immersive.push(Qt.resolvedUrl(path), props)
            if (App.profiling) App.profilePlace(path)
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

        function addRepository(link) { repositorySheet.start(link) }
        function toast(message, kind, action) { toasts.show(message, kind, action) }

        function land(target) { hero.land(target) }

        function resume(entry) {
            if (!Router.mediaAllowed(entry.media)) { Router.refuseMedia(); return }
            const props = { url: entry.sourceUrl, summary: entry, autoResume: true }
            if (entry.media === "anime" && String(entry.source || "").startsWith("aniyomi:")) {
                push("pages/AnimeDetails.qml", { sourceUrl: entry.sourceUrl, summary: entry, autoResume: true })
            } else if (entry.media === "anime") {
                const extra = entry.extra || {}
                const aid = extra.anilistId || entry.anilistId || (entry.id ? parseInt(String(entry.id).split(":")[2]) : 0)
                push("pages/AnimeDetails.qml", { anilistId: aid, summary: entry, autoResume: true })
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
        visible: !window.immersiveOpen || immersive.busy
        // Disabling drops focus; take it back when the reader closes.
        onEnabledChanged: if (enabled) forceActiveFocus()
        Keys.onEscapePressed: (event) => event.accepted = shell.back()

        // Everything the dock floats over. The dock samples this for its glass.
        Item {
            id: stage
            anchors.fill: parent

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
                z: floating ? 2 : 1
                width: visible ? implicitWidth : 0
                x: side === "right" ? parent.width - width - window.navInset : window.navInset
                y: window.navInset
                height: parent.height - 2 * window.navInset
                current: shell.section
                backdropItem: content
                onNavigate: (name) => shell.goSection(name)
                onOpenPalette: commandPalette.open()
            }

            Item {
                id: content
                anchors.left: window.sidebarShown && window.navSide === "left" ? sidebar.right : parent.left
                anchors.right: window.sidebarShown && window.navSide === "right" ? sidebar.left : parent.right
                anchors.top: parent.top
                // The bottom dock floats over the page rather than sitting on
                // a shelf, so its glass has something to frost.
                anchors.bottom: Theme.compact ? tabBar.top : parent.bottom
                anchors.topMargin: dock.visible && window.navSide === "top" ? dock.height + 18 : 0
                clip: true

                Repeater {
                    id: sectionRepeater
                    model: ["home", "comics", "novels", "anime", "library", "history", "activity", "downloads", "settings"]
                        .filter(k => Router.sectionAllowed(k))
                    delegate: Loader {
                        id: sectionLoader
                        required property string modelData
                        readonly property string key: modelData
                        readonly property bool current: shell.section === key
                        anchors.fill: parent
                        active: shell.visited[key] === true
                        asynchronous: shell.settled
                        visible: opacity > 0.001
                        opacity: current ? 1 : 0
                        scale: current ? 1 : 0.99
                        z: current ? 1 : 0
                        transform: Translate { y: sectionLoader.current ? 0 : 12; Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate } } }
                        Behavior on opacity { NumberAnimation { duration: sectionLoader.current ? Motion.slow : Motion.fast; easing.type: Easing.OutCubic } }
                        Behavior on scale { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate } }

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
        backdropItem: stage
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
        // The window's own root, not ApplicationWindow's content, which Qt
        // (6.9 and later) pads by the safe area: inside it a reader could not
        // reach under the status bar and was told its inset was 0, so its
        // hidden top bar stuck out above it, over the phone's clock.
        parent: window.contentItem.parent
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

    // Incognito badge over the pages, at the bottom (readers say it in
    // their own header).
    AbstractButton {
        id: incognitoBadge
        parent: shellView
        z: 4
        visible: App.settings.incognito === true
        anchors.horizontalCenter: parent.horizontalCenter
        // Above the phone's tabs or a bottom dock, clear of page headers.
        y: parent.height - height - 12 - (tabBar.visible ? tabBar.height
            : dock.visible && dock.side !== "top" ? parent.height - dock.y : 0)
        height: 28
        width: badgeRow.implicitWidth + 22
        hoverEnabled: true
        Accessible.name: qsTr("Incognito is on. Turn it off")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Nothing you read or watch is recorded. Click to turn off.")
        onClicked: Router.setIncognito(false)
        background: LiquidGlass {
            zTier: 1
            radius: height / 2
            backdropItem: stage
            interactive: true
            externalHovered: incognitoBadge.hovered
            externalPressed: incognitoBadge.pressed
            trackPointer: false
            iconName: "venetian-mask"
            iconSize: 15
            iconColor: Theme.text
            text: qsTr("Incognito")
            textColor: Theme.text
            fontSize: Theme.textXs
            bold: true
        }
        contentItem: Item {
            Row {
                id: badgeRow
                anchors.centerIn: parent
                spacing: 6
                Icon { anchors.verticalCenter: parent.verticalCenter; name: "venetian-mask"; size: 15; color: Theme.text }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Incognito")
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs
                    font.weight: Font.DemiBold
                }
            }
        }
    }

    CommandPalette {
        id: commandPalette
        z: 30
    }

    LockScreen {}

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
    AddRepositoryDialog { id: repositorySheet }
    NotificationsPanel { id: notificationsPanel }
    Timer { interval: 1500; running: true; onTriggered: shell.settled = true }
    Timer {
        id: notifyHook
        interval: App.startAction.notify ? (App.startAction.notify.delay || 0) : 0
        onTriggered: Notifications.add("app", App.startAction.notify.title || "Test", App.startAction.notify.body || "", true)
    }

    // Android 13 and later: ask once, after the welcome, to allow
    // notifications (new chapters, announcements, updates).
    Timer {
        interval: 1200
        running: App.platform === "android" && App.settings.onboarded === true && App.settings.notificationsAsked !== true && !onboarding.active
        onTriggered: {
            App.setSetting("notificationsAsked", true)
            Notifications.askPermission()
        }
    }

    // Android: once, after the welcome, ask to be let off battery
    // optimisation, which stops update checks, downloads and reading aloud
    // soon after the app leaves the screen. Settings → Library has it later.
    ConfirmDialog {
        id: batteryAsk
        heading: qsTr("Let SCANCORE run in the background?")
        message: qsTr("Android's battery optimisation closes SCANCORE soon after you leave it, so new chapters are not checked, downloads pause and reading aloud stops. Android will ask you to allow it; you can change it later in Settings → Library.")
        confirmText: qsTr("Allow")
        onConfirmed: App.askBatteryUnrestricted()
    }
    Timer {
        interval: 2500
        running: App.platform === "android" && App.batteryRestricted && App.settings.onboarded === true
            && App.settings.batteryAsked !== true && !onboarding.active
            && App.settings.notificationsAsked === true && Qt.application.state === Qt.ApplicationActive
        onTriggered: {
            App.setSetting("batteryAsked", true)
            batteryAsk.open()
        }
    }
    Toasts {
        id: toasts
        z: 40
        // Above the phone's tab bar, not over it.
        bottomInset: (tabBar.visible ? tabBar.height : 0) + (incognitoBadge.visible ? incognitoBadge.height + 12 : 0)
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
    Shortcut { sequences: Keymap.sequences("comics"); enabled: Router.sectionAllowed("comics"); onActivated: shell.goSection("comics") }
    Shortcut { sequences: Keymap.sequences("novels"); enabled: Router.sectionAllowed("novels"); onActivated: shell.goSection("novels") }
    Shortcut { sequences: Keymap.sequences("anime"); onActivated: shell.goSection("anime") }
    Shortcut { sequences: Keymap.sequences("library"); onActivated: shell.goSection("library") }
    Shortcut { sequences: Keymap.sequences("history"); enabled: Router.sectionAllowed("history"); onActivated: shell.goSection("history") }
    Shortcut { sequences: Keymap.sequences("activity"); onActivated: shell.goSection("activity") }
    Shortcut { sequences: Keymap.sequences("downloads"); enabled: Router.sectionAllowed("downloads"); onActivated: shell.goSection("downloads") }
    Shortcut { sequences: Keymap.sequences("settings"); onActivated: shell.goSection("settings") }
    Shortcut {
        sequences: Keymap.sequences("fullscreen")
        onActivated: window.visibility === Window.FullScreen ? window.showNormal() : window.showFullScreen()
    }
}
