import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Music (a preview): anime openings, endings and inserts from AnimeThemes,
// searched by song, singer or anime, and this season's, grouped by anime.
// Lyrics come with each song in Now Playing.
Item {
    id: page
    property string query: ""
    property var searchTask: null
    property var seasonTask: Music.animeSongsAvailable ? Music.seasonSongs() : null
    readonly property var found: searchTask && searchTask.ok ? searchTask.result : []
    // This season's, one list with the anime as sections.
    readonly property var season: {
        if (!seasonTask || !seasonTask.ok) return []
        const out = []
        for (const a of seasonTask.result) for (const s of a.songs) out.push(s)
        return out
    }
    readonly property var shown: query.length ? found : season

    function run(text) {
        query = text.trim()
        searchTask = query.length ? Music.searchSongs(query) : null
    }

    ListView {
        id: list
        anchors.fill: parent
        model: page.shown
        clip: true
        maximumFlickVelocity: Motion.flickVelocity
        flickDeceleration: Motion.flickDeceleration
        bottomMargin: 96
        WheelScroll {}
        section.property: page.query.length ? "" : "anime"
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section
            x: Theme.gutter
            width: list.width - 2 * Theme.gutter
            topPadding: 18
            bottomPadding: 6
            text: section
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        header: Column {
            width: list.width
            spacing: Math.round(Theme.sectionGap * 0.6)
            bottomPadding: 8
            PageHeader {
                title: qsTr("Music")
                subtitle: qsTr("Anime openings and endings, with their lyrics · preview")
            }
            SearchField {
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                large: true
                placeholderText: qsTr("Search a song, a singer or an anime")
                onSubmitted: (text) => page.run(text)
                onTextChanged: if (text.length === 0 && page.query.length > 0) page.run("")
            }
            // What is playing, a tap from Now Playing.
            Rectangle {
                visible: MusicQueue.active
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                height: 76
                radius: Theme.retro ? 0 : Theme.radiusLg
                color: Theme.retro ? Theme.tile : Theme.alpha(Theme.accent, Theme.dark ? 0.14 : 0.09)
                Cover {
                    id: nowArt
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 52; height: 52
                    radius: Theme.retro ? 0 : Theme.radiusSm
                    source: MusicQueue.current ? MusicQueue.current.cover || "" : ""
                }
                Column {
                    anchors.left: nowArt.right
                    anchors.leftMargin: 14
                    anchors.right: openNow.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: qsTr("Now playing"); color: Theme.retro ? Theme.tileInk : Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.weight: Font.DemiBold }
                    Text { width: parent.width; text: MusicQueue.current ? MusicQueue.current.title : ""; color: Theme.retro ? Theme.tileInk : Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.textMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                }
                ScButton {
                    id: openNow
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    variant: "primary"
                    iconName: "type"
                    text: qsTr("Lyrics")
                    onClicked: Router.openNowPlaying()
                }
            }
            Row {
                x: Theme.gutter
                spacing: 10
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.query.length ? qsTr("Songs for “%1”").arg(page.query) : qsTr("This season")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textXl
                    font.weight: Font.DemiBold
                }
                ScButton {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: page.shown.length > 0
                    compact: true
                    variant: "ghost"
                    iconName: "play"
                    text: qsTr("Play all")
                    onClicked: MusicQueue.playList(page.shown, 0)
                }
            }
            StateView {
                width: parent.width
                visible: !Music.animeSongsAvailable || (page.query.length ? !!page.searchTask && !page.searchTask.ok : !!page.seasonTask && !page.seasonTask.ok)
                    || (page.query.length && page.searchTask && page.searchTask.ok && page.found.length === 0)
                task: page.query.length ? page.searchTask : page.seasonTask
                empty: !Music.animeSongsAvailable || (page.query.length > 0 && !!page.searchTask && page.searchTask.ok && page.found.length === 0)
                emptyTitle: Music.animeSongsAvailable ? qsTr("No songs found") : qsTr("Anime songs are turned off for now")
                emptyText: qsTr("Try the anime's name in romaji, or the singer.")
                emptyIcon: "search"
                onRetry: page.query.length ? page.run(page.query) : (page.seasonTask = Music.seasonSongs())
            }
        }
        delegate: SongRow {
            required property var modelData
            required property int index
            x: Theme.gutter - 8
            width: Math.min(list.width - 2 * Theme.gutter + 16, 776)
            song: modelData
            onPlayed: MusicQueue.playList(page.shown, index)
        }
        // Retro and phones scroll with the finger; this keeps the wheel too.
        ScrollBar.vertical: ScScrollBar {}
    }
}
