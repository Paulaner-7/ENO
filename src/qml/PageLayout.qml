pragma Singleton
import QtQuick
import Scancore

// How the user arranged each page (Home, Comics, Novels, Anime): the order
// of its sections, which are hidden, and each section's look. Kept as JSON
// in the setting "layout.<page>" (lists in settings come back as Qt lists,
// and a string survives sync as it is):
//   { "order": ["continue", "trending", …],
//     "hidden": { "popular": true },
//     "look": { "continue": { "style": "grid", "size": "large", "count": 12,
//                             "subtitles": false, "progress": false } },
//     "removed": { "comics": true },
//     "extras": [{ "key": "src:<source>:latest", "source": "<source>",
//                  "listing": "latest", "label": "Comix · Latest" }] }
// A removed section is gone (and loads nothing) until added back; extras
// are shelves of a source's own listing that the user added.
// Sections a page adds later appear after the arranged ones.
QtObject {
    id: layouts

    // Bumped on every change, for bindings that read through functions.
    property int revision: 0

    readonly property var defaultLook: ({ style: "row", size: "medium", count: 0, subtitles: true, progress: true })

    function stored(page) {
        revision
        try {
            const value = App.settings["layout." + page]
            const parsed = typeof value === "string" && value.length ? JSON.parse(value) : null
            return parsed && typeof parsed === "object" ? parsed : {}
        } catch (e) {
            return {}
        }
    }

    function save(page, data) {
        App.setSetting("layout." + page, JSON.stringify(data))
        revision++
    }

    // The page's section keys in the user's order: `keys` is the order the
    // page declares them in.
    function order(page, keys) {
        const saved = stored(page).order || []
        const known = Array.from(keys)
        const out = saved.filter(k => known.indexOf(k) >= 0)
        // A section the user has not placed goes right after the one it
        // follows by default (or first).
        known.forEach((k, i) => {
            if (out.indexOf(k) >= 0) return
            const after = i > 0 ? out.indexOf(known[i - 1]) : -1
            out.splice(after + 1, 0, k)
        })
        return out
    }

    function isHidden(page, key) { return !!(stored(page).hidden || {})[key] }

    function look(page, key) {
        const saved = (stored(page).look || {})[key] || {}
        return Object.assign({}, defaultLook, saved)
    }

    function isArranged(page) {
        const s = stored(page)
        return !!(s.order || s.hidden || s.look || s.removed || s.extras)
    }

    function setHidden(page, key, hidden) {
        const s = stored(page)
        s.hidden = s.hidden || {}
        if (hidden) s.hidden[key] = true
        else delete s.hidden[key]
        save(page, s)
    }

    function isRemoved(page, key) { return !!(stored(page).removed || {})[key] }

    function setRemoved(page, key, removed) {
        const s = stored(page)
        s.removed = s.removed || {}
        if (removed) s.removed[key] = true
        else delete s.removed[key]
        if (s.hidden) delete s.hidden[key]
        save(page, s)
    }

    function extras(page) { return stored(page).extras || [] }

    function addExtra(page, extra) {
        const s = stored(page)
        s.extras = (s.extras || []).filter(e => e.key !== extra.key).concat([extra])
        save(page, s)
    }

    function removeExtra(page, key) {
        const s = stored(page)
        s.extras = (s.extras || []).filter(e => e.key !== key)
        if (s.order) s.order = s.order.filter(k => k !== key)
        if (s.look) delete s.look[key]
        save(page, s)
    }

    // Moves `key` to position `index` among `keys` (the page's sections).
    function moveTo(page, keys, key, index) {
        const s = stored(page)
        const list = order(page, keys)
        const from = list.indexOf(key)
        if (from < 0) return
        list.splice(from, 1)
        list.splice(Math.max(0, Math.min(index, list.length)), 0, key)
        s.order = list
        save(page, s)
    }

    function move(page, keys, key, delta) {
        moveTo(page, keys, key, order(page, keys).indexOf(key) + delta)
    }

    function setLook(page, key, property, value) {
        const s = stored(page)
        s.look = s.look || {}
        const current = Object.assign({}, s.look[key] || {})
        if (value === defaultLook[property]) delete current[property]
        else current[property] = value
        if (Object.keys(current).length) s.look[key] = current
        else delete s.look[key]
        save(page, s)
    }

    function reset(page) {
        App.setSetting("layout." + page, "")
        revision++
    }
}
