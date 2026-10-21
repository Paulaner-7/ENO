import QtQuick
import ScanCore

SettingsPane {
    id: pane
    title: qsTr("About")
    subtitle: qsTr("Version, updates and help")
    iconName: "info"

    readonly property string updateLine: Updater.managedByFlatpak ? qsTr("Updates come through Flatpak.")
        : Updater.state === "checking" ? qsTr("Checking upstream SCANCORE for updates…")
        : Updater.state === "current" ? qsTr("Synced with the latest upstream SCANCORE release.")
        : Updater.state === "available" ? qsTr("Upstream SCANCORE ") + Updater.version + qsTr(" is available — ask Antigravity to sync ENO.")
        : Updater.state === "downloading" ? qsTr("Downloading ") + Updater.version + " · " + Math.round(Updater.progress * 100) + "%"
        : Updater.state === "ready" ? qsTr("Version ") + Updater.version + qsTr(" is ready to install.")
        : Updater.state === "error" ? Updater.error
        : qsTr("Custom ENO build · Forked from SCANCORE.")

    // A key drawn as a keycap.
    component Key: Rectangle {
        property alias text: label.text
        width: Math.max(26, label.implicitWidth + 14)
        height: 24
        radius: 6
        color: Theme.surface2
        border.width: 1
        border.color: Theme.borderStrong
        Text {
            id: label
            anchors.centerIn: parent
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs + 1
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        width: parent.width
        height: card.height + 40
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Flow {
            id: card
            // The buttons go under the name when beside it would squeeze it.
            readonly property bool stacked: Theme.compact || width - updateButtons.wanted - 16 < 300
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 16
            Row {
                spacing: 16
                width: card.stacked ? card.width : Math.max(0, card.width - updateButtons.width - 16)
                Rectangle {
                    width: 56
                    height: 56
                    radius: 16
                    color: Theme.dark ? "#000000" : "#141416"
                    border.width: 1
                    border.color: Theme.borderStrong
                    Image {
                        anchors.centerIn: parent
                        height: 38
                        width: Math.round(height * (592 / 792))
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/branding/eno-symbol-dark.png"
                        smooth: true
                        mipmap: true
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 72
                    spacing: 4
                    Row {
                        spacing: 8
                        Text {
                            text: "ENO"
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.textXl
                            font.weight: Font.Bold
                            font.letterSpacing: 1.5
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: version.implicitWidth + 14
                            height: 20
                            radius: 10
                            color: Theme.surface2
                            border.width: 1
                            border.color: Theme.border
                            Text {
                                id: version
                                anchors.centerIn: parent
                                text: App.version
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textXs
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: pane.updateLine
                        color: Updater.state === "error" ? Theme.danger : Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                }
            }
            // Wraps under the name on phones and in a narrow pane, so
            // "Update" and "Check for updates" never run out of the card.
            Flow {
                id: updateButtons
                readonly property real wanted: (updateNow.visible ? updateNow.implicitWidth + spacing : 0) + checkNow.implicitWidth
                visible: !Updater.managedByFlatpak
                width: card.stacked ? card.width : wanted
                spacing: 8
                ScButton {
                    id: updateNow
                    visible: Updater.state === "available" || Updater.state === "ready"
                    variant: "primary"
                    text: Updater.state === "ready" ? qsTr("Restart to update") : Updater.canInstall ? qsTr("Update") : qsTr("Release notes")
                    onClicked: Updater.canInstall ? (Updater.state === "ready" ? Updater.install() : Updater.download()) : App.openUrl(Updater.pageUrl)
                }
                ScButton {
                    id: checkNow
                    text: qsTr("Check for updates")
                    iconName: "refresh-cw"
                    loading: Updater.state === "checking"
                    enabled: Updater.state !== "downloading"
                    onClicked: Updater.check(false)
                }
            }
        }
    }

    SettingsGroup {
        visible: !Updater.managedByFlatpak
        title: qsTr("Updates")
        SettingRow {
            title: qsTr("Check for upstream SCANCORE updates")
            description: qsTr("Notifies you when a new official SCANCORE release is out so Antigravity can merge it into ENO without losing your changes.")
            Toggle { checked: App.settings.checkAppUpdates !== false; onToggled: App.setSetting("checkAppUpdates", checked) }
        }
    }

    SettingsGroup {
        title: qsTr("Help and community")
        SettingRow { iconName: "globe"; title: "scancore.onl"; description: qsTr("The SCANCORE website"); clickable: true; external: true; onClicked: App.openUrl("https://scancore.onl") }
        SettingRow { iconName: "message-circle"; title: qsTr("Discord"); description: qsTr("Questions, feedback and updates"); clickable: true; external: true; onClicked: App.openUrl("https://scancore.onl/discord") }
        SettingRow { iconName: "triangle-alert"; title: qsTr("Report a problem"); description: qsTr("Opens GitHub Issues"); clickable: true; external: true; onClicked: App.openUrl("https://github.com/medium-gh/scancore/issues/new/choose") }
        SettingRow { iconName: "file-text"; title: qsTr("Crash reports and diagnostics"); description: qsTr("To send to the developer when something goes wrong"); clickable: true; onClicked: crashReports.showAll() }
        SettingRow { iconName: "sparkles"; title: qsTr("Welcome tour"); description: qsTr("See the introduction again"); clickable: true; onClicked: Router.shell.showWelcome() }
        SettingRow { iconName: "file-text"; title: qsTr("Licences"); description: qsTr("The open-source software inside SCANCORE"); clickable: true; onClicked: licences.start() }
    }

    LicencesDialog { id: licences }
    CrashReportsDialog { id: crashReports }

    SettingsGroup {
        visible: !Theme.compact
        title: qsTr("Keyboard shortcuts")
        Grid {
            x: 16
            width: parent.width - 32
            topPadding: 14
            bottomPadding: 14
            columns: 2
            columnSpacing: 28
            rowSpacing: 10
            Repeater {
                model: [{ keys: ["Ctrl", "K"], label: qsTr("Search everything") }, { keys: ["Esc"], label: qsTr("Back, or close the reader") },
                        { keys: ["Ctrl", "1–5"], label: qsTr("Switch section") }, { keys: ["Alt", "←"], label: qsTr("Back") },
                        { keys: ["Ctrl", "B"], label: qsTr("Collapse the sidebar") }, { keys: ["F11"], label: qsTr("Full screen") },
                        { keys: ["N", "P"], label: qsTr("Next or previous chapter") }, { keys: ["M"], label: qsTr("Strip or pages (comics)") },
                        { keys: ["Space"], label: qsTr("Scroll a screen") }, { keys: ["+", "−"], label: qsTr("Width or text size") }]
                Row {
                    required property var modelData
                    width: (parent.width - 28) / 2
                    spacing: 10
                    Row {
                        width: 92
                        spacing: 4
                        Repeater {
                            model: modelData.keys
                            Key { required property string modelData; text: modelData }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 102
                        text: modelData.label
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Text {
        width: parent.width
        leftPadding: 4
        wrapMode: Text.Wrap
        text: qsTr("MIT licence · github.com/medium-gh/scancore. Fonts: DM Sans, Space Grotesk, Literata (OFL). Icons: Lucide (ISC). HTML parsing: lexbor (Apache 2.0). Video player: mpv and FFmpeg (LGPL), built by tools/libmpv/build.sh.")
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs + 1
    }
}
