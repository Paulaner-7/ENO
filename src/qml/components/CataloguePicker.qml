import QtQuick
import Scancore

// The tracker catalogue a page browses and searches, per media (settings
// "catalogComic", "catalogNovel" and "catalogAnime"). Comick has only
// comics; MyAnimeList needs SCANCORE's client id and is left out of builds
// without it. Anime always browse a catalogue (AniList or MyAnimeList).
Segmented {
    property string media: "comic"
    readonly property string key: media === "novel" ? "catalogNovel" : media === "anime" ? "catalogAnime" : "catalogComic"

    options: (media === "comic" ? [{ value: "comick", label: qsTr("Comick") }] : [])
        .concat([{ value: "anilist", label: "AniList" }])
        .concat(Trackers.malAvailable ? [{ value: "mal", label: "MyAnimeList" }] : [])
        .concat(media === "anime" ? [] : [{ value: "none", label: qsTr("None") }])
    value: App.settings[key] || (media === "comic" ? "comick" : "anilist")
    onPicked: (v) => App.setSetting(key, v)
}
