import QtQuick
import Scancore

// Tiles packed the way Windows Phone's Start packed them: a grid of square
// units, small tiles 1×1, medium 2×2 (on even columns), wide 4×2, each in
// the first free place reading left to right, top to bottom. The list is
// short by design (fixed tiles plus what the user pinned), so a Repeater is
// fine here.
Item {
    id: grid
    property var tiles: []
    property int columns: 4
    property int gap: 8
    property int beat: 0
    property bool live: true
    // 0 at rest; 1 swung away (turnstileOut), each column a little later.
    property real turn: 0

    readonly property real unit: Math.max(1, Math.floor((width - gap * (columns - 1)) / columns))
    readonly property var places: pack(tiles, columns)
    implicitHeight: places.rows * (unit + gap) - (places.rows > 0 ? gap : 0)

    signal clicked(var tile)
    signal held(var tile, Item item)

    function spanOf(size) {
        if (size === "wide") return { w: Math.min(4, grid.columns), h: 2 }
        if (size === "medium") return { w: Math.min(2, grid.columns), h: 2 }
        return { w: 1, h: 1 }
    }

    // [{x, y, w, h}] in units, and the rows used.
    function pack(list, cols) {
        const taken = []
        const free = (r, c, w, h) => {
            for (let i = r; i < r + h; ++i)
                for (let j = c; j < c + w; ++j)
                    if (taken[i] && taken[i][j]) return false
            return true
        }
        const out = []
        let rows = 0
        for (const t of list) {
            const s = spanOf(t.size)
            let placed = null
            for (let r = 0; !placed; ++r)
                for (let c = 0; c + s.w <= cols && !placed; c += s.w)
                    if (free(r, c, s.w, s.h)) placed = { x: c, y: r, w: s.w, h: s.h }
            for (let i = placed.y; i < placed.y + placed.h; ++i) {
                taken[i] = taken[i] || []
                for (let j = placed.x; j < placed.x + placed.w; ++j) taken[i][j] = true
            }
            rows = Math.max(rows, placed.y + placed.h)
            out.push(placed)
        }
        return { list: out, rows: rows }
    }

    Repeater {
        model: grid.tiles
        delegate: Tile {
            id: placed
            required property var modelData
            required property int index
            readonly property var place: grid.places.list[index] || { x: 0, y: 0, w: 1, h: 1 }
            tile: modelData
            beat: grid.beat
            live: grid.live
            column: place.x
            x: place.x * (grid.unit + grid.gap)
            y: place.y * (grid.unit + grid.gap)
            width: place.w * grid.unit + (place.w - 1) * grid.gap
            height: place.h * grid.unit + (place.h - 1) * grid.gap
            // Columns further right swing a little later.
            turn: Math.max(0, Math.min(1, grid.turn * 1.7 - place.x * 0.1 - place.y * 0.04)) * 90
            onClicked: grid.clicked(modelData)
            onHeld: grid.held(modelData, placed)
        }
    }
}
