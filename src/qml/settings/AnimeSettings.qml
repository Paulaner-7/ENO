import QtQuick
import ScanCore

SettingsPane {
    title: qsTr("Anime")
    subtitle: qsTr("Playback and skipping")
    iconName: "tv"

    SettingsGroup {
        title: qsTr("Player")
        note: qsTr("Episodes resume where you stopped. Skip times come from AniSkip when someone has submitted them.")
        SettingRow {
            iconName: "skip-forward"
            title: qsTr("Play the next episode automatically")
            description: qsTr("Starts after a short countdown you can cancel.")
            Toggle { checked: App.settings.autoNext !== false; onToggled: App.setSetting("autoNext", checked) }
        }
        SettingRow {
            iconName: "clock"
            title: qsTr("Countdown before the next episode")
            enabled: App.settings.autoNext !== false
            opacity: enabled ? 1 : 0.45
            Segmented {
                options: [{ value: 3, label: "3 s" }, { value: 8, label: "8 s" }, { value: 15, label: "15 s" }]
                value: Number(App.settings.nextCountdown) || 8
                onPicked: (v) => App.setSetting("nextCountdown", v)
            }
        }
        SettingRow {
            iconName: "rotate-ccw"
            title: qsTr("Skip intros and recaps automatically")
            description: qsTr("Otherwise a Skip button appears while they play.")
            Toggle { checked: App.settings.autoSkip === true; onToggled: App.setSetting("autoSkip", checked) }
        }
        SettingRow {
            iconName: "skip-forward"
            title: qsTr("Skip endings automatically")
            description: qsTr("Goes straight to the next episode's countdown when the credits start.")
            Toggle { checked: App.settings.autoSkipEndings === true; onToggled: App.setSetting("autoSkipEndings", checked) }
        }
        SettingRow {
            iconName: "captions"
            title: qsTr("Default subtitle language")
            description: qsTr("Automatically selects Italian subtitles when available.")
            Segmented {
                options: [{ value: "it", label: qsTr("Italian") }, { value: "en", label: qsTr("English") }, { value: "orig", label: qsTr("Original") }]
                value: App.settings.animeSubLang || "it"
                onPicked: (v) => App.setSetting("animeSubLang", v)
            }
        }
        SettingRow {
            iconName: "sparkles"
            title: qsTr("Subtitle quality")
            description: qsTr("Render resolution for ASS/SSA styled subtitles.")
            Segmented {
                options: [{ value: "high", label: qsTr("High") }, { value: "medium", label: qsTr("Medium") }, { value: "low", label: qsTr("Low") }]
                value: App.settings.animeSubQuality || "high"
                onPicked: (v) => App.setSetting("animeSubQuality", v)
            }
        }
        SettingRow {
            iconName: "video"
            title: qsTr("Default video quality")
            description: qsTr("Selects the highest available stream resolution (e.g. 1080p).")
            Segmented {
                options: [{ value: "max", label: qsTr("Highest") }, { value: "720", label: "720p" }, { value: "auto", label: qsTr("Auto") }]
                value: App.settings.animeVideoQuality || "max"
                onPicked: (v) => App.setSetting("animeVideoQuality", v)
            }
        }
    }

    // SCANCORE's own player (anime from extensions); web players draw their own.
    SettingsGroup {
        visible: Anime.nativePlayerAvailable
        title: qsTr("Subtitles")
        note: qsTr("For SCANCORE's player. Subtitles with their own styling keep it, apart from the size.")
        SettingRow {
            iconName: "zoom-in"
            title: qsTr("Size")
            Segmented {
                options: [{ value: 0.8, label: qsTr("Small") }, { value: 1, label: qsTr("Normal") }, { value: 1.25, label: qsTr("Large") }, { value: 1.5, label: qsTr("Huge") }]
                value: Number(App.settings.subtitleScale) || 1
                onPicked: (v) => App.setSetting("subtitleScale", v)
            }
        }
        SettingRow {
            iconName: "palette"
            title: qsTr("Colour")
            Segmented {
                options: [{ value: "#ffffff", label: qsTr("White") }, { value: "#ffe14d", label: qsTr("Yellow") }, { value: "#b8f5ff", label: qsTr("Light blue") }]
                value: App.settings.subtitleColor || "#ffffff"
                onPicked: (v) => App.setSetting("subtitleColor", v)
            }
        }
        SettingRow {
            iconName: "type"
            title: qsTr("Outline")
            enabled: App.settings.subtitleBox !== true
            opacity: enabled ? 1 : 0.45
            Segmented {
                options: [{ value: 1.5, label: qsTr("Thin") }, { value: 3, label: qsTr("Normal") }, { value: 5, label: qsTr("Thick") }]
                value: Number(App.settings.subtitleOutline)
                onPicked: (v) => App.setSetting("subtitleOutline", v)
            }
        }
        SettingRow {
            iconName: "square"
            title: qsTr("Dark box behind the lines")
            description: qsTr("Easier to read over bright scenes.")
            Toggle { checked: App.settings.subtitleBox === true; onToggled: App.setSetting("subtitleBox", checked) }
        }
    }

    SettingsGroup {
        visible: Anime.nativePlayerAvailable
        title: qsTr("Picture")
        SettingRow {
            iconName: "sparkles"
            title: qsTr("Sharpen (Anime4K)")
            description: qsTr("Cleaner lines and upscaling for lower-resolution episodes. Best needs a strong graphics card; on phones Fast is the most to try.")
            Segmented {
                options: [{ value: "off", label: qsTr("Off") }, { value: "fast", label: qsTr("Fast") }, { value: "hq", label: qsTr("Best") }]
                value: App.settings.animeSharpen || "off"
                onPicked: (v) => App.setSetting("animeSharpen", v)
            }
        }
    }
}
