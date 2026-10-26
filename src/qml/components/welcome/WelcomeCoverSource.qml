import QtQuick
import Scancore

// Covers for the welcome: AniList's trending anime, asked for as the
// welcome opens. Until (or unless) they come, the welcome draws its own
// (WelcomeArt).
QtObject {
    id: source
    property bool active: true
    property var animeTask: active ? Anime.home() : null

    readonly property var anime: {
        if (!animeTask || !animeTask.ok) return []
        const out = [], seen = new Set()
        for (const key of ["trending", "popular", "top"])
            for (const it of animeTask.result[key] || [])
                if (it && it.cover && !it.isAdult && !seen.has(it.cover)) { seen.add(it.cover); out.push(it.cover) }
        return out
    }
    readonly property var all: anime

    // The cover for a seat in a wall or a picture; "" while none came.
    function pick(seed, from) {
        const list = from === "comic" ? [] : anime
        return list.length ? list[Math.abs(seed * 7 + 3) % list.length] : ""
    }
}
