import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes
import ScanCore

// First-run welcome: the interface language, what SCANCORE does, the tracker
// to keep, extensions (a repository link, if the reader has one), a quick
// look-and-feel choice, an optional scancore.onl account, and a few tips. Shown until finished or
// skipped; Settings can show it again.
Rectangle {
    id: root
    property int step: App.startAction.onboardingStep || 0
    property int direction: 1
    readonly property int steps: 7
    // The account step, which has its own Continue.
    readonly property int accountIndex: 5
    signal finished()

    color: Theme.base
    focus: true

    function go(next) {
        if (next < 0 || next >= steps) return
        direction = next > step ? 1 : -1
        step = next
    }
    function finish() {
        App.setSetting("onboarded", true)
        leave.start()
    }

    // Nothing behind the welcome (the tabs, the page) takes taps or wheels.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    Keys.onRightPressed: if (step < steps - 1 && step !== accountIndex) go(step + 1)
    Keys.onLeftPressed: go(step - 1)
    Keys.onEscapePressed: finish()

    // Slow drifting light behind everything, so the screen never feels static.
    Item {
        anchors.fill: parent
        clip: true
        Repeater {
            model: 3
            Shape {
                id: glow
                required property int index
                width: root.width * 1.1
                height: width
                preferredRendererType: Shape.CurveRenderer
                x: root.width * [0.45, -0.45, 0.1][index]
                y: root.height * [-0.55, 0.25, 0.7][index]
                SequentialAnimation on x {
                    running: !Motion.reduced && root.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: root.width * [0.25, -0.25, 0.3][glow.index]; duration: 16000 + glow.index * 3000; easing.type: Easing.InOutSine }
                    NumberAnimation { to: root.width * [0.45, -0.45, 0.1][glow.index]; duration: 16000 + glow.index * 3000; easing.type: Easing.InOutSine }
                }
                ShapePath {
                    strokeWidth: -1
                    fillGradient: RadialGradient {
                        centerX: glow.width / 2; centerY: glow.height / 2
                        centerRadius: glow.width / 2
                        focalX: centerX; focalY: centerY
                        GradientStop { position: 0; color: Theme.alpha(Theme.text, Theme.dark ? 0.075 : 0.06) }
                        GradientStop { position: 1; color: Theme.alpha(Theme.text, 0) }
                    }
                    PathRectangle { width: glow.width; height: glow.height }
                }
            }
        }
    }

    // ------------------------------------------------------------------ steps
    Item {
        id: stage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.topMargin: parent.SafeArea.margins.top

        Repeater {
            id: pages
            model: [welcomeStep, mediaStep, trackerStep, extensionsStep, styleStep, accountStep, doneStep]
            delegate: Loader {
                id: pageLoader
                required property var modelData
                required property int index
                readonly property bool current: root.step === index
                anchors.fill: parent
                sourceComponent: modelData
                active: Math.abs(root.step - index) <= 1
                visible: opacity > 0.01
                opacity: current ? 1 : 0
                z: current ? 1 : 0
                enabled: current
                transform: Translate {
                    x: pageLoader.current ? 0 : (pageLoader.index < root.step ? -60 : 60)
                    Behavior on x { NumberAnimation { duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
                }
                Behavior on opacity { NumberAnimation { duration: pageLoader.current ? Motion.page : Motion.normal; easing.type: Easing.OutCubic } }
            }
        }
    }

    // ------------------------------------------------------------------ footer
    Item {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.SafeArea.margins.bottom
        height: Theme.compact ? 112 : 88

        // Phones give the dots a row of their own, well above the buttons;
        // beside them the buttons would cover the last dots.
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Theme.compact ? 14 : (parent.height - height) / 2
            spacing: 8
            Repeater {
                model: root.steps
                Rectangle {
                    required property int index
                    width: index === root.step ? 28 : 8
                    height: 8
                    radius: 4
                    color: index === root.step ? Theme.text : Theme.alpha(Theme.text, index < root.step ? 0.45 : 0.18)
                    Behavior on width { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Motion.normal } }
                }
            }
        }
        ScButton {
            anchors.left: parent.left
            anchors.leftMargin: Theme.gutter - (Theme.compact ? 12 : 0)
            // Centred on the same line as the 36 px button opposite.
            y: Theme.compact ? parent.height - 18 - 36 + (36 - height) / 2 : (parent.height - height) / 2
            visible: root.step < root.steps - 1
            variant: Theme.compact ? "plain" : "ghost"
            compact: Theme.compact
            text: root.step === 0 ? qsTr("Skip intro") : qsTr("Back")
            onClicked: root.step === 0 ? root.finish() : root.go(root.step - 1)
        }
        ScButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.gutter
            y: Theme.compact ? parent.height - height - 18 : (parent.height - height) / 2
            visible: root.step < root.steps - 1
            variant: root.step === root.accountIndex ? "ghost" : "primary"
            compact: Theme.compact
            implicitHeight: Theme.compact ? 36 : 40
            iconName: root.step === root.accountIndex ? "" : "chevron-right"
            iconAfter: true
            text: root.step === 0 ? qsTr("Get started") : root.step === root.accountIndex ? qsTr("Skip for now") : qsTr("Continue")
            onClicked: root.go(root.step + 1)
        }
    }

    // ------------------------------------------------------------------ content
    component Heading: Column {
        property string kicker
        property string title
        property string body
        spacing: 12
        width: Math.min(parent ? parent.width - 2 * Theme.gutter : 600, 640)
        Text {
            visible: text.length > 0
            text: parent.kicker
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            font.weight: Font.Bold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 2
        }
        Text {
            width: parent.width
            text: parent.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.compact ? 30 : 42
            font.weight: Font.Bold
            font.letterSpacing: -1
            wrapMode: Text.Wrap
            lineHeight: 1.05
        }
        Text {
            visible: text.length > 0
            width: parent.width
            text: parent.body
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textLg
            lineHeight: 1.35
            wrapMode: Text.Wrap
        }
    }

    // 1. Welcome: the mark draws in with a scan line sweeping across it.
    Component {
        id: welcomeStep
        Item {
            Column {
                anchors.centerIn: parent
                spacing: 28
                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Theme.compact ? 220 : 300
                    height: width / 2.02
                    Image {
                        id: mark
                        anchors.fill: parent
                        source: Theme.dark ? "qrc:/branding/eno-logo-dark.png" : "qrc:/branding/eno-logo-light.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: 0
                        scale: 0.92
                        Component.onCompleted: intro.start()
                    }
                    Rectangle {
                        id: scan
                        width: parent.width * 1.3
                        x: -parent.width * 0.15
                        height: 3
                        radius: 2
                        color: Theme.text
                        opacity: 0
                        y: 0
                    }
                    SequentialAnimation {
                        id: intro
                        PauseAnimation { duration: 200 }
                        ParallelAnimation {
                            NumberAnimation { target: scan; property: "opacity"; to: 0.9; duration: 200 }
                            NumberAnimation { target: scan; property: "y"; from: 0; to: mark.height; duration: Motion.reduced ? 0 : 900; easing.type: Easing.InOutCubic }
                            NumberAnimation { target: mark; property: "opacity"; to: 1; duration: Motion.reduced ? 0 : 900; easing.type: Easing.InOutCubic }
                            NumberAnimation { target: mark; property: "scale"; to: 1; duration: Motion.reduced ? 0 : 1100; easing.type: Easing.OutCubic }
                        }
                        NumberAnimation { target: scan; property: "opacity"; to: 0; duration: 300 }
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "S C A N C O R E"
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.compact ? 18 : 22
                    font.weight: Font.DemiBold
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(root.width - 2 * Theme.gutter, 520)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("Comics, novels and anime in one fast app. Everything runs and stays on your device.")
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textLg
                    lineHeight: 1.35
                }
                // The interface language comes first, so the rest of the
                // intro is in it. It switches right away.
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6
                        Icon { anchors.verticalCenter: parent.verticalCenter; name: "languages"; size: 14; color: Theme.textFaint }
                        Text {
                            text: qsTr("Language")
                            color: Theme.textFaint
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textXs
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 1.5
                        }
                    }
                    Select {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(root.width - 2 * Theme.gutter, 260)
                        implicitHeight: 42
                        Accessible.name: qsTr("Language")
                        model: [{ value: "system", label: qsTr("System") }].concat(Customize.languages)
                        value: Customize.language
                        onPicked: (v) => Customize.setLanguage(v)
                    }
                }
            }
        }
    }

    // 2. What it does.
    Component {
        id: mediaStep
        Flickable {
            WheelScroll {}
            contentHeight: mediaColumn.height + 60
            clip: true
            Column {
                id: mediaColumn
                x: Theme.gutter
                y: Theme.compact ? 24 : Math.max(40, (root.height - footer.height - height) / 2 - 20)
                width: parent.width - 2 * Theme.gutter
                spacing: 28
                Heading {
                    kicker: qsTr("Everything in one place")
                    title: qsTr("Read and watch from the sources you choose")
                    body: qsTr("Search every source together, keep your place in every series, and download chapters for offline reading.")
                }
                Flow {
                    width: parent.width
                    spacing: 16
                    Repeater {
                        model: [
                            { icon: "book-open", title: qsTr("Comics"), text: qsTr("Manhwa, manga and webtoons from Webtoon, MangaDex, your files and the sources you add. Vertical strip or page by page.") },
                            { icon: "book-text", title: qsTr("Novels"), text: qsTr("Royal Road, your EPUB books and the sources you add, with a reader you can tune: fonts, spacing, paper and sepia themes.") },
                            { icon: "tv", title: qsTr("Anime"), text: qsTr("Trending, seasonal and top lists from AniList, a weekly schedule, and episodes that resume where you stopped.") }
                        ]
                        Rectangle {
                            id: card
                            required property var modelData
                            required property int index
                            width: Theme.compact ? parent.width : (parent.width - 32) / 3
                            height: cardColumn.implicitHeight + 40
                            radius: Theme.radiusLg
                            color: Theme.surface
                            border.width: 1
                            border.color: Theme.border
                            opacity: 0
                            transform: Translate { id: lift; y: 24 }
                            Component.onCompleted: show.start()
                            SequentialAnimation {
                                id: show
                                PauseAnimation { duration: 120 + card.index * 110 }
                                ParallelAnimation {
                                    NumberAnimation { target: card; property: "opacity"; to: 1; duration: Motion.slow }
                                    NumberAnimation { target: lift; property: "y"; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
                                }
                            }
                            Column {
                                id: cardColumn
                                x: 20
                                y: 20
                                width: parent.width - 40
                                spacing: 12
                                Rectangle {
                                    width: 44; height: 44; radius: 12
                                    color: Theme.surface2
                                    Icon { anchors.centerIn: parent; name: card.modelData.icon; size: 22; color: Theme.text }
                                }
                                Text { text: card.modelData.title; color: Theme.text; font.family: Theme.fontDisplay; font.pixelSize: Theme.textXl; font.weight: Font.DemiBold }
                                Text { width: parent.width; text: card.modelData.text; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textMd; wrapMode: Text.Wrap; lineHeight: 1.3 }
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Trackers: the catalogue each page browses, and the trackers to keep
    // in step. Every connected tracker gets progress for every media.
    Component {
        id: trackerStep
        Flickable {
            WheelScroll {}
            contentHeight: trackerColumn.height + 60
            clip: true
            Column {
                id: trackerColumn
                x: Theme.gutter
                y: Theme.compact ? 24 : Math.max(40, (root.height - footer.height - height) / 2 - 20)
                width: Math.min(parent.width - 2 * Theme.gutter, 640)
                spacing: 24
                Heading {
                    kicker: qsTr("Trackers")
                    title: qsTr("Browse and track your way")
                    body: qsTr("Pick the catalogue each page browses and searches. Connected trackers follow what you finish, whichever you browse with.")
                }
                Column {
                    spacing: 10
                    Text { text: qsTr("Comics"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    CataloguePicker { media: "comic" }
                }
                Column {
                    spacing: 10
                    Text { text: qsTr("Novels"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    CataloguePicker { media: "novel" }
                }
                Column {
                    spacing: 10
                    visible: Trackers.malAvailable
                    Text { text: qsTr("Anime"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    CataloguePicker { media: "anime" }
                }
                Column {
                    width: parent.width
                    spacing: 4
                    visible: Trackers.services.length > 0
                    Text { text: qsTr("Keep your lists in step"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    Repeater {
                        // The three most used; the others are in Settings → Connections.
                        model: Trackers.services.filter(t => ["anilist", "mal", "comick"].indexOf(t.key) >= 0).map(t => ({ service: t.key }))
                        Item {
                            required property var modelData
                            width: trackerColumn.width
                            height: Math.max(48, connector.height + 12)
                            Column {
                                anchors.left: parent.left
                                anchors.right: connector.left
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                Text { text: connector.label; color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.textMd; font.weight: Font.DemiBold }
                                Text {
                                    width: parent.width
                                    visible: text.length > 0
                                    text: connector.status
                                    color: Theme.textMuted
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.textSm
                                    wrapMode: Text.Wrap
                                }
                            }
                            TrackerConnect {
                                id: connector
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                service: modelData.service
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. Extensions: other people's sources, from a repository the reader
    // already has a link to. Nothing is suggested.
    Component {
        id: extensionsStep
        Flickable {
            id: extensionsPage
            WheelScroll {}
            contentHeight: extensionsColumn.height + 60
            clip: true
            readonly property var repositories: { App.dependOn(Extensions.revision); return Extensions.repositories() }
            Column {
                id: extensionsColumn
                x: Theme.gutter
                y: Theme.compact ? 24 : Math.max(40, (root.height - footer.height - height) / 2 - 20)
                width: Math.min(parent.width - 2 * Theme.gutter, 640)
                spacing: 22
                Heading {
                    kicker: qsTr("Extensions")
                    title: qsTr("Have a repository link?")
                    body: qsTr("SCANCORE runs extensions made for Mihon and Tachiyomi. Paste a repository link and SCANCORE shows what it holds before adding anything. Extensions are other people's code, so you choose whether to trust each one.")
                }
                // Optional: the built-in sources work without any of this.
                Rectangle {
                    width: parent.width
                    height: optionalText.implicitHeight + 24
                    radius: Theme.radiusMd
                    color: Theme.alpha(Theme.accent, 0.08)
                    border.width: 1
                    border.color: Theme.alpha(Theme.accent, 0.25)
                    Icon {
                        id: optionalIcon
                        x: 14
                        y: 13
                        name: "info"
                        size: 16
                        color: Theme.accent
                    }
                    Text {
                        id: optionalText
                        anchors.left: optionalIcon.right
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        y: 12
                        text: qsTr("This is optional. SCANCORE comes with its own built-in sources for comics, novels and anime, so you can skip this step and add a repository later if you want more.")
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                }
                Row {
                    width: parent.width
                    spacing: 10
                    SearchField {
                        id: linkField
                        width: parent.width - addLink.width - 10
                        placeholderText: qsTr("Repository link")
                        onSubmitted: (text) => { if (text.trim()) Router.addRepository(text.trim()) }
                    }
                    ScButton {
                        id: addLink
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Add")
                        iconName: "plus"
                        enabled: linkField.text.trim().length > 0
                        onClicked: Router.addRepository(linkField.text.trim())
                    }
                }
                Text {
                    width: parent.width
                    text: extensionsPage.repositories.length
                        ? qsTr("Added: %1").arg(extensionsPage.repositories.map(r => r.name).join(", "))
                        : qsTr("No link? Continue; the Extensions page on the Comics tab takes one any time.")
                    color: extensionsPage.repositories.length ? Theme.success : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    // 5. Look and feel, applied live.
    Component {
        id: styleStep
        Flickable {
            contentHeight: styleColumn.height + 60
            clip: true
            Column {
                id: styleColumn
                x: Theme.gutter
                y: Theme.compact ? 24 : Math.max(40, (root.height - footer.height - height) / 2 - 20)
                width: Math.min(parent.width - 2 * Theme.gutter, 640)
                spacing: 26
                Heading {
                    kicker: qsTr("Make it yours")
                    title: qsTr("Pick a look")
                    body: qsTr("Changes apply right away. You can adjust these and much more in Settings.")
                }
                Column {
                    spacing: 10
                    Text { text: qsTr("Theme"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    Segmented {
                        options: [{ value: "system", label: qsTr("System"), icon: "monitor" }, { value: "light", label: qsTr("Light"), icon: "sun" },
                                  { value: "dark", label: qsTr("Dark"), icon: "moon" }, { value: "black", label: qsTr("Black") }]
                        value: App.settings.theme
                        onPicked: (v) => App.setSetting("theme", v)
                    }
                }
                Column {
                    spacing: 10
                    Text { text: qsTr("Accent"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    Row {
                        spacing: 14
                        Repeater {
                            model: ["mono", "violet", "ember", "sky", "rose"]
                            Rectangle {
                                required property string modelData
                                width: 36; height: 36; radius: 18
                                color: Theme.dark ? Theme.accents[modelData].dark : Theme.accents[modelData].light
                                scale: App.settings.accent === modelData ? 1.12 : 1
                                Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutBack } }
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: -4; radius: width / 2
                                    color: "transparent"; border.width: 2; border.color: Theme.text
                                    visible: App.settings.accent === modelData
                                }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.setSetting("accent", modelData) }
                            }
                        }
                    }
                }
                Column {
                    spacing: 10
                    Text { text: qsTr("Comics open as"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    Segmented {
                        options: [{ value: "strip", label: qsTr("Vertical strip"), icon: "move-vertical" }, { value: "paged", label: qsTr("Pages"), icon: "layers" }]
                        value: App.settings.readerMode
                        onPicked: (v) => App.setSetting("readerMode", v)
                    }
                }
                Column {
                    spacing: 10
                    Text { text: qsTr("Comic sources in"); color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
                    Select {
                        Accessible.name: qsTr("Comic sources in")
                        model: [{ value: "", label: qsTr("All languages") }].concat(Comics.languages())
                        value: App.settings.sourceLanguages || ""
                        onPicked: (v) => App.setSetting("sourceLanguages", v)
                    }
                }
                Row {
                    spacing: 12
                    Toggle { checked: App.settings.reducedMotion === true; onToggled: App.setSetting("reducedMotion", checked) }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Reduce motion"); color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.textMd }
                }
            }
        }
    }

    // 6. Account.
    Component {
        id: accountStep
        Flickable {
            contentHeight: accountRow.height + 60
            clip: true
            Item {
                id: accountRow
                width: parent.width
                height: Math.max(accountIntro.height, accountForm.height) + (Theme.compact ? accountIntro.height + 24 : 0)
                y: Theme.compact ? 24 : Math.max(40, (root.height - footer.height - height) / 2 - 20)
                Heading {
                    id: accountIntro
                    x: Theme.gutter
                    width: Theme.compact ? parent.width - 2 * Theme.gutter : Math.min(420, parent.width / 2 - Theme.gutter)
                    kicker: qsTr("Optional")
                    title: Account.signedIn ? qsTr("You're signed in") : qsTr("Bring your scancore.onl account")
                    body: Account.signedIn
                        ? qsTr("Signed in as ") + (Account.user.username || Account.user.email) + qsTr(". Your website library and reading history are syncing into this app.")
                        : qsTr("Sign in to bring your website library and comic reading history here and keep both in step. Everything works without an account too.")
                }
                AuthForm {
                    id: accountForm
                    visible: !Account.signedIn
                    x: Theme.compact ? Theme.gutter : Math.max(accountIntro.x + accountIntro.width + 48, parent.width / 2)
                    y: Theme.compact ? accountIntro.height + 24 : 0
                    width: Theme.compact ? parent.width - 2 * Theme.gutter : Math.min(400, parent.width - x - Theme.gutter)
                    onDone: root.go(root.accountIndex + 1)
                }
                ScButton {
                    visible: Account.signedIn
                    x: Theme.gutter
                    y: accountIntro.height + 24
                    variant: "primary"
                    text: qsTr("Continue")
                    onClicked: root.go(root.accountIndex + 1)
                }
            }
        }
    }

    // 7. Done.
    Component {
        id: doneStep
        Item {
            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * Theme.gutter, 560)
                spacing: 24
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 72; height: 72; radius: 36
                    color: Theme.accent
                    scale: 0.4
                    Component.onCompleted: pop.start()
                    NumberAnimation on scale { id: pop; running: false; to: 1; duration: Motion.page; easing.type: Easing.OutBack }
                    Icon { anchors.centerIn: parent; name: "check"; size: 34; color: Theme.accentInk }
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("You're all set")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.compact ? 30 : 40
                    font.weight: Font.Bold
                }
                Column {
                    width: parent.width
                    spacing: 12
                    Repeater {
                        model: (Theme.touch ? [] : [{ icon: "command", text: qsTr("Press Ctrl+K to search everything or jump anywhere.") }]).concat([
                            { icon: "bookmark", text: qsTr("Add series to your library and SCANCORE checks them for new chapters.") },
                            { icon: "download", text: qsTr("Download chapters to read without a connection.") },
                            { icon: "settings", text: qsTr("Reader layout, novel typography and themes live in Settings.") }
                        ])
                        Row {
                            required property var modelData
                            spacing: 14
                            width: parent.width
                            Rectangle {
                                width: 36; height: 36; radius: 10
                                color: Theme.surface2
                                Icon { anchors.centerIn: parent; name: modelData.icon; size: 17; color: Theme.textMuted }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 50
                                text: modelData.text
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    variant: "primary"
                    text: qsTr("Start exploring")
                    iconName: "chevron-right"
                    onClicked: root.finish()
                }
            }
        }
    }

    ParallelAnimation {
        id: leave
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: Motion.slow; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "scale"; to: 1.04; duration: Motion.slow; easing.type: Easing.OutCubic }
        onFinished: root.finished()
    }
}
