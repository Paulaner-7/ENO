import QtQuick
import ScanCore

SettingsPane {
    id: pane
    title: qsTr("Library")
    subtitle: qsTr("New chapters, notifications and background work")
    iconName: "library"

    SettingsGroup {
        title: qsTr("Updates")
        SettingRow {
            iconName: "refresh-cw"
            title: qsTr("Check for new chapters")
            description: qsTr("Saved comics and novels are checked in the background while SCANCORE is open.")
            Select {
                model: [{ value: 0, label: qsTr("Never") }, { value: 1, label: qsTr("Every hour") }, { value: 2, label: qsTr("Every 2 hours") },
                        { value: 6, label: qsTr("Every 6 hours") }, { value: 24, label: qsTr("Once a day") }]
                value: App.settings.libraryCheckHours
                onPicked: (v) => App.setSetting("libraryCheckHours", v)
            }
        }
        SettingRow {
            iconName: "bell"
            title: qsTr("Notify me about new chapters")
            description: qsTr("They are always listed under Notifications; this also shows them in the system's notifications.")
            Toggle { checked: App.settings.notifyUpdates !== false; onToggled: App.setSetting("notifyUpdates", checked) }
        }
        SettingRow {
            iconName: "list"
            title: qsTr("One notification per check")
            description: qsTr("Sums up every new chapter a check found, instead of one notification per series.")
            Toggle { checked: App.settings.notifyDigest === true; onToggled: App.setSetting("notifyDigest", checked) }
        }
        SettingRow {
            iconName: "megaphone"
            title: qsTr("Announcements from SCANCORE")
            description: qsTr("News about SCANCORE from scancore.onl, in Notifications as soon as they are posted.")
            Toggle { checked: App.settings.announcements !== false; onToggled: App.setSetting("announcements", checked) }
        }
    }

    // Library lists as chips that switch a list setting's members.
    component ListChips: Flow {
        id: chips
        property string setting
        readonly property var chosen: App.settings[setting] || []
        width: Math.min(520, pane.width - 120)
        spacing: 6
        Repeater {
            model: Router.libraryCategories("comic")
            Chip {
                required property var modelData
                text: modelData.label
                selected: chips.chosen.indexOf(modelData.value) >= 0
                onClicked: {
                    const next = chips.chosen.filter(v => v !== modelData.value)
                    if (!selected) next.push(modelData.value)
                    App.setSetting(chips.setting, next)
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Which series are checked")
        SettingRow {
            iconName: "list-checks"
            title: qsTr("Lists")
            description: qsTr("Only series on these lists of your library are checked for new chapters.")
            wide: true
            ListChips { setting: "updateLists" }
        }
        SettingRow {
            iconName: "check"
            title: qsTr("Skip finished series")
            description: qsTr("Series their site marks as completed.")
            Toggle { checked: App.settings.updateSkipFinished !== false; onToggled: App.setSetting("updateSkipFinished", checked) }
        }
        SettingRow {
            iconName: "book-open"
            title: qsTr("Skip series with chapters left to read")
            description: qsTr("Checked again once you catch up.")
            Toggle { checked: App.settings.updateSkipUnread === true; onToggled: App.setSetting("updateSkipUnread", checked) }
        }
        SettingRow {
            iconName: "clock"
            title: qsTr("Skip series you have not started")
            Toggle { checked: App.settings.updateSkipNotStarted === true; onToggled: App.setSetting("updateSkipNotStarted", checked) }
        }
        SettingRow {
            iconName: "wifi-off"
            title: qsTr("Check by itself only on Wi-Fi")
            description: qsTr("Not on mobile data or a metered connection. Checking by hand always works.")
            Toggle { checked: App.settings.updateOnlyUnmetered === true; onToggled: App.setSetting("updateOnlyUnmetered", checked) }
        }
        SettingRow {
            visible: App.platform === "android"
            iconName: "battery-warning"
            title: qsTr("Check by itself only while charging")
            Toggle { checked: App.settings.updateOnlyCharging === true; onToggled: App.setSetting("updateOnlyCharging", checked) }
        }
    }

    SettingsGroup {
        title: qsTr("Downloads")
        SettingRow {
            iconName: "download"
            title: qsTr("Download new chapters")
            description: (App.settings.autoDownloadLists || []).length
                ? qsTr("New chapters of series on these lists download as soon as they are found.")
                : qsTr("Pick lists whose new chapters download as soon as they are found.")
            wide: true
            ListChips { setting: "autoDownloadLists" }
        }
        SettingRow {
            iconName: "trash"
            title: qsTr("Delete chapters once read")
            description: qsTr("A downloaded chapter goes when you finish the next one.")
            Toggle { checked: App.settings.deleteAfterRead === true; onToggled: App.setSetting("deleteAfterRead", checked) }
        }
    }

    SettingsGroup {
        visible: App.platform !== "android"
        title: qsTr("Background")
        SettingRow {
            iconName: "minimize"
            title: qsTr("Keep running in the tray when the window closes")
            description: qsTr("Lets update checks and downloads continue in the background.")
            Toggle { checked: App.settings.closeToTray === true; onToggled: App.setSetting("closeToTray", checked) }
        }
    }

    SettingsGroup {
        visible: App.platform === "android"
        title: qsTr("Background")
        SettingRow {
            iconName: App.batteryRestricted ? "battery-warning" : "check"
            title: App.batteryRestricted ? qsTr("Battery optimisation stops SCANCORE in the background")
                                         : qsTr("SCANCORE can run in the background")
            description: App.batteryRestricted
                ? qsTr("Android closes it soon after you leave it, so new chapters are not checked, downloads pause and reading aloud stops.")
                : qsTr("Update checks, downloads and reading aloud carry on when you leave the app.")
            ScButton {
                visible: App.batteryRestricted
                text: qsTr("Allow")
                onClicked: App.askBatteryUnrestricted()
            }
        }
    }
}
