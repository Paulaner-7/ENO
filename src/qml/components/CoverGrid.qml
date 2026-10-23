import QtQuick
import ScanCore

// Responsive grid of cover cards for use inside a scrolling page. Only the
// rows on screen (and a couple either side) exist: cards are reused in a
// ring as the page scrolls, so hundreds of results cost what one screen
// does. Every row is as tall as a card with a two-line title.
Item {
    id: grid
    property var items: []
    property int minCardWidth: Math.round((Theme.compact ? 104 : 150) * Theme.coverScale)
    property int spacing: Theme.compact ? 12 : 20
    property real coverRatio: 1.5
    // Titles over the covers (a compact grid), not under them.
    property bool titlesInside: false
    property var subtitleFor: (item) => ""
    property var badgeFor: (item) => ""
    property var progressFor: (item) => -1
    // Whether a card is picked in a selection.
    property var markedFor: (item) => false
    signal opened(var item, Item coverItem)

    // Covers rise in when the grid first fills; refreshes and filtering after
    // that show them in place instead of replaying the entrance.
    property bool settled: false
    onItemsChanged: if (items.length > 0 && !settled) Qt.callLater(() => grid.settled = true)

    readonly property int columns: Math.max(2, Math.floor((width + spacing) / (minCardWidth + spacing)))
    readonly property real cardWidth: (width - (columns - 1) * spacing) / columns
    // Cover, gap, two title lines and the subtitle (CoverCard's layout).
    readonly property real cardHeight: titlesInside ? Math.ceil(cardWidth * coverRatio)
                                                    : Math.ceil(cardWidth * coverRatio + 10 + titleMetrics.height * 2.2 + 2 + subtitleMetrics.height)
    readonly property real rowStride: cardHeight + spacing
    readonly property int rows: Math.ceil(items.length / columns)

    width: parent ? parent.width - 2 * Theme.gutter : 800
    x: Theme.gutter
    height: rows > 0 ? rows * rowStride - spacing : 0

    FontMetrics { id: titleMetrics; font.family: Theme.fontSans; font.pixelSize: Theme.textMd; font.weight: Font.DemiBold }
    FontMetrics { id: subtitleMetrics; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }

    // The page that scrolls this grid, if any.
    readonly property Flickable view: {
        for (let p = grid.parent; p; p = p.parent)
            if (p instanceof Flickable) return p as Flickable
        return null
    }
    // Rows kept around the screen, so a fling shows covers, not gaps.
    readonly property int bufferRows: 2
    readonly property int windowRows: view ? Math.ceil(view.height / Math.max(1, rowStride)) + 1 + 2 * bufferRows : rows
    readonly property int slots: Math.min(items.length, windowRows * columns)
    // Where the grid sits in the page; anything above it moving changes the
    // page's height, so that is what this follows.
    readonly property real offsetInView: {
        if (!view) return 0
        void grid.y
        void view.contentHeight
        return grid.mapToItem(view.contentItem, 0, 0).y
    }
    readonly property int firstRow: {
        if (!view || rows === 0) return 0
        const top = view.contentY - offsetInView
        return Math.max(0, Math.min(rows - windowRows, Math.floor(top / rowStride) - bufferRows))
    }
    readonly property int firstIndex: Math.max(0, firstRow) * columns

    Repeater {
        model: grid.slots
        delegate: CoverCard {
            id: card
            required property int index
            // The one item in the window that falls on this slot.
            readonly property int itemIndex: {
                const offset = ((card.index - grid.firstIndex) % grid.slots + grid.slots) % grid.slots
                return grid.firstIndex + offset
            }
            readonly property var entry: card.itemIndex < grid.items.length ? grid.items[card.itemIndex] : null
            visible: card.entry !== null
            x: (card.itemIndex % grid.columns) * (grid.cardWidth + grid.spacing)
            y: Math.floor(card.itemIndex / grid.columns) * grid.rowStride
            width: grid.cardWidth
            item: card.entry || ({})
            animateIn: !grid.settled
            coverRatio: grid.coverRatio
            titleInside: grid.titlesInside
            subtitle: card.entry ? grid.subtitleFor(card.entry) : ""
            badge: card.entry ? grid.badgeFor(card.entry) : ""
            progress: card.entry ? grid.progressFor(card.entry) : -1
            marked: card.entry ? grid.markedFor(card.entry) : false
            order: card.itemIndex
            onOpened: (it, cover) => grid.opened(it, cover)
        }
    }
}
