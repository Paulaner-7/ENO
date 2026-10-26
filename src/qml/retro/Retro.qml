pragma Singleton
import QtQuick
import Scancore

// The Retro mode's own state: the hub's sections, the tiles pinned to it,
// the title and where the hub is (for the app bar's button). Lists are kept
// as JSON strings (settings hand lists back as Qt lists; a string syncs as
// it is).
QtObject {
    id: retro

    readonly property bool upperCase: App.settings.retroCase !== "lower"
    readonly property string title: typeof App.settings.retroTitle === "string" && App.settings.retroTitle.trim().length
        ? App.settings.retroTitle.trim() : "scancore"
    readonly property bool gradientTitle: App.settings.retroTitleColor !== "plain"
    // "recent": the art of what was read last; "theme": the custom theme's
    // picture (or nothing); "none".
    readonly property string background: ["recent", "theme", "none"].indexOf(App.settings.retroBackground) >= 0
        ? App.settings.retroBackground : "recent"
    readonly property bool liveTiles: App.settings.retroLiveTiles !== false

    // Where the hub is, and whether it is showing: the app bar's button
    // does what the section in view is for.
    property string hubSection: "explore"
    property bool onHub: true

    readonly property var sectionKeys: ["explore", "recent", "library", "anime"]
    readonly property var defaultSections: ["explore", "recent", "library", "anime"]

    function sectionLabel(key) {
        switch (key) {
        case "explore": return qsTr("Explore")
        case "recent": return qsTr("Recent")
        case "library": return qsTr("Library")
        case "anime": return qsTr("Anime")
        }
        return key
    }

    function parse(text, fallback) {
        try {
            const out = text.length ? JSON.parse(text) : null
            return Array.isArray(out) ? out : fallback
        } catch (e) {
            return fallback
        }
    }

    // Strings first: App.settings changes with every setting, a string
    // only when it is different, so the lists below stay the same objects
    // (the hub would rebuild its sections otherwise).
    readonly property string sectionsText: typeof App.settings.retroSections === "string" ? App.settings.retroSections : ""
    readonly property string pinsText: typeof App.settings.retroTiles === "string" ? App.settings.retroTiles : ""

    // The hub's sections in the user's order (at least one).
    readonly property var sectionList: {
        const keys = parse(sectionsText, defaultSections).filter(k => sectionKeys.indexOf(k) >= 0)
        return (keys.length ? keys : defaultSections).map(k => ({ key: k, label: sectionLabel(k) }))
    }
    function sections() { return sectionList }
    function setSections(keys) { App.setSetting("retroSections", JSON.stringify(keys)) }

    // Pinned tiles: [{ id, size, item }], item being the series map it opens.
    readonly property var pinList: parse(pinsText, []).filter(p => p && p.id && p.item)
    function pins() { return pinList }
    function savePins(list) { App.setSetting("retroTiles", JSON.stringify(list)) }
    function isPinned(id) { return pinList.some(p => p.id === id) }
    // What a tile needs to open the series again, nothing more.
    function pin(item, size) {
        if (!item || !item.id) return
        const keep = {}
        for (const k of ["id", "media", "source", "sourceUrl", "url", "title", "cover", "referer", "anilistId", "extra"])
            if (item[k] !== undefined && item[k] !== null && item[k] !== "") keep[k] = item[k]
        const list = pins().filter(p => p.id !== item.id)
        list.unshift({ id: item.id, size: size || "medium", item: keep })
        savePins(list)
        Router.toast(qsTr("Pinned to the hub"), "success")
    }
    function unpin(id) { savePins(pins().filter(p => p.id !== id)) }
    function resize(id, size) { savePins(pins().map(p => p.id === id ? Object.assign({}, p, { size: size }) : p)) }
    function move(id, by) {
        const list = pins()
        const i = list.findIndex(p => p.id === id)
        const j = i + by
        if (i < 0 || j < 0 || j >= list.length) return
        const t = list[i]
        list[i] = list[j]
        list[j] = t
        savePins(list)
    }
}
