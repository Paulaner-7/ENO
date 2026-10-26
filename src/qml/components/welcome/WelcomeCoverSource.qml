import QtQuick
import Scancore

// Covers for the welcome: Comick's most followed series rated safe and
// AniList's trending anime, asked for as the welcome opens. Until (or
// unless) they come, the welcome draws its own (WelcomeArt).
QtObject {
    id: source
    property bool active: true
    property var comicTask: active ? Comics.catalog("", { sort: "user_follow_count", content: { safe: 1 } }) : null
    property var animeTask: active ? Anime.home() : null

    readonly property var comics: {
        if (!comicTask || !comicTask.ok) return []
        const out = []
        for (const it of comicTask.result.comics || [])
            if (it.cover && !it.adult) out.push(it.cover)
        return out
    }
    readonly property var anime: {
        if (!animeTask || !animeTask.ok) return []
        const out = [], seen = new Set()
        for (const key of ["trending", "popular", "top"])
            for (const it of animeTask.result[key] || [])
                if (it && it.cover && !it.isAdult && !seen.has(it.cover)) { seen.add(it.cover); out.push(it.cover) }
        return out
    }
    // Comics and anime in turn.
    readonly property var all: {
        const out = [], seen = new Set()
        const n = Math.max(comics.length, anime.length)
        for (let i = 0; i < n; ++i)
            for (const c of [comics[i], anime[i]])
                if (c && !seen.has(c)) { seen.add(c); out.push(c) }
        return out
    }

    // The cover for a seat in a wall or a picture; "" while none came.
    function pick(seed, from) {
        const list = from === "comic" ? comics : from === "anime" ? anime : all
        return list.length ? list[Math.abs(seed * 7 + 3) % list.length] : ""
    }
}
