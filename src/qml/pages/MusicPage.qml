import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Scancore

// Music (a preview): anime openings, endings and inserts from AnimeThemes,
// searched by song, singer or anime, and this season's, grouped by anime;
// or your own files, album by album, from the folders chosen here; or the
// music extensions you installed, each one's songs under its name. Lyrics
// come with each song in Now Playing.
Item {
    id: page
    property string query: ""
    property var searchTask: null
    property var seasonTask: null
    // Where the songs come from: "anime" (AnimeThemes), "files" or
    // "extensions".
    readonly property string from: App.settings.musicFrom || (Music.animeSongsAvailable ? "anime" : "files")
    readonly property bool files: from === "files"
    readonly property bool extensions: from === "extensions"
    readonly property var sources: { App.dependOn(Extensions.revision); return Music.sources() }
    // One task per extension: its popular songs, or what the search found.
    property var sourceTasks: []
    readonly property var fromSources: {
        const out = []
        for (const t of sourceTasks) if (t.task.ok) for (const s of t.task.result) out.push(s)
        return out
    }
    readonly property bool sourcesLoading: sourceTasks.some(t => t.task.loading)
    // Every extension failed: the first one's error.
    readonly property var sourceError: sourceTasks.length && sourceTasks.every(t => !t.task.loading && !t.task.ok) ? sourceTasks[0].task : null
    function askSources() {
        sourceTasks = sources.map(s => ({ key: s.key, task: query.length ? Music.searchSource(s.key, query) : Music.popularSource(s.key) }))
    }
    readonly property var folders: Array.from(App.settings.musicFolders || [])
    property var localTask: null
    readonly property var mine: localTask && localTask.ok ? localTask.result : []
    // Your files are filtered here, as you type.
    readonly property var mineFound: {
        if (!query.length) return mine
        const q = query.toLowerCase()
        return mine.filter(s => (s.title + " " + (s.artists || []).join(" ") + " " + s.album).toLowerCase().includes(q))
    }
    function load() {
        if (extensions) askSources()
        else if (files) localTask = folders.length ? Music.localSongs() : null
        else if (!seasonTask && Music.animeSongsAvailable) seasonTask = Music.seasonSongs()
    }
    Component.onCompleted: load()
    onFromChanged: load()
    onFoldersChanged: if (files) load()
    onSourcesChanged: if (extensions) askSources()
    // "content://…/tree/primary%3AMusic" is "Music".
    function folderName(path) {
        return decodeURIComponent(path).replace(/[\/\\:]+$/, "").replace(/^.*[\/\\:]/, "") || path
    }
    readonly property var found: searchTask && searchTask.ok ? searchTask.result : []
    // This season's, one list with the anime as sections.
    readonly property var season: {
        if (!seasonTask || !seasonTask.ok) return []
        const out = []
        for (const a of seasonTask.result) for (const s of a.songs) out.push(s)
        return out
    }
    readonly property var shown: extensions ? fromSources : files ? mineFound : query.length ? found : season

    function run(text) {
        query = text.trim()
        searchTask = query.length && !files && !extensions ? Music.searchSongs(query) : null
        if (extensions) askSources()
    }

    FolderDialog {
        id: folderDialog
        onAccepted: {
            const path = App.localPath(selectedFolder)
            if (page.folders.indexOf(path) < 0) App.setSetting("musicFolders", page.folders.concat([path]))
        }
    }
    ActionMenu { id: folderMenu }

    ListView {
        id: list
        anchors.fill: parent
        model: page.shown
        clip: true
        maximumFlickVelocity: Motion.flickVelocity
        flickDeceleration: Motion.flickDeceleration
        bottomMargin: 96
        WheelScroll {}
        // The songs arrive after the header is laid out, and a new model
        // left the view below the title: until the reader scrolls, stay at
        // the very top (as History does).
        property bool touched: false
        onMovementStarted: touched = true
        onModelChanged: if (!touched) Qt.callLater(() => { if (!list.touched) list.positionViewAtBeginning() })
        section.property: page.extensions ? "sourceName" : page.files ? "album" : page.query.length ? "" : "anime"
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
            onHeightChanged: if (!list.touched) Qt.callLater(() => { if (!list.touched) list.positionViewAtBeginning() })
            spacing: Math.round(Theme.sectionGap * 0.6)
            bottomPadding: 8
            PageHeader {
                title: qsTr("Music")
                subtitle: qsTr("Songs with their lyrics · preview")
            }
            Segmented {
                visible: !Theme.retro
                x: Theme.gutter
                // Phones: the three share the width, without icons.
                fill: page.width < 520
                width: fill ? parent.width - 2 * Theme.gutter : implicitWidth
                options: fill ? [{ value: "anime", label: qsTr("Anime songs") }, { value: "files", label: qsTr("Your files") },
                                 { value: "extensions", label: qsTr("Extensions") }]
                              : [{ value: "anime", label: qsTr("Anime songs"), icon: "tv" }, { value: "files", label: qsTr("Your files"), icon: "folder-open" },
                                 { value: "extensions", label: qsTr("Extensions"), icon: "puzzle" }]
                value: page.from
                onPicked: (v) => App.setSetting("musicFrom", v)
            }
            PivotHeaders {
                visible: Theme.retro
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                height: visible ? implicitHeight : 0
                options: [{ value: "anime", label: qsTr("Anime songs") }, { value: "files", label: qsTr("Your files") },
                          { value: "extensions", label: qsTr("Extensions") }]
                value: page.from
                onPicked: (v) => App.setSetting("musicFrom", v)
            }
            SearchField {
                id: search
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                large: true
                visible: page.files ? page.folders.length > 0 : page.extensions ? page.sources.length > 0 : true
                placeholderText: page.files ? qsTr("Search your songs") : page.extensions ? qsTr("Search your music extensions")
                               : qsTr("Search a song, a singer or an anime")
                onSubmitted: (text) => page.run(text)
                onTextChanged: if (page.files || (text.length === 0 && page.query.length > 0)) page.run(text)
            }
            // The folders listed, and adding another.
            Flow {
                visible: page.files
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                spacing: 8
                Repeater {
                    model: page.files ? page.folders : []
                    Chip {
                        id: folderChip
                        required property string modelData
                        iconName: "folder-open"
                        text: page.folderName(modelData)
                        onClicked: folderMenu.openAt(folderChip, folderChip.width / 2, folderChip.height, [
                            { text: qsTr("Stop listing this folder"), icon: "x",
                              run: () => App.setSetting("musicFolders", page.folders.filter(f => f !== modelData)) }
                        ])
                    }
                }
                Chip {
                    iconName: "plus"
                    text: page.folders.length ? qsTr("Add another folder") : qsTr("Add a music folder")
                    onClicked: folderDialog.open()
                }
                Chip {
                    visible: page.folders.length > 0
                    iconName: "refresh-cw"
                    text: qsTr("Look again")
                    enabled: !(page.localTask && page.localTask.loading)
                    onClicked: page.load()
                }
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
                    text: page.extensions ? (page.query.length ? qsTr("Songs for “%1”").arg(page.query) : qsTr("Popular"))
                        : page.files ? (page.query.length ? qsTr("Songs for “%1”").arg(page.query) : qsTr("Your songs"))
                        : page.query.length ? qsTr("Songs for “%1”").arg(page.query) : qsTr("This season")
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
                visible: page.files && (!page.folders.length || !!page.localTask && !page.localTask.ok
                                        || (!!page.localTask && page.localTask.ok && page.mineFound.length === 0))
                task: page.files ? page.localTask : null
                empty: !page.folders.length || (!!page.localTask && page.localTask.ok && page.mineFound.length === 0)
                emptyTitle: !page.folders.length ? qsTr("Add a folder with your music")
                          : page.query.length ? qsTr("No songs found") : qsTr("No music in these folders")
                emptyText: qsTr("MP3, FLAC, Ogg, Opus and M4A files, with their covers, and lyrics from .lrc files beside them. Nothing is copied.")
                emptyIcon: "folder-open"
                onRetry: page.load()
            }
            // Extensions: none yet, or nothing found.
            StateView {
                width: parent.width
                visible: page.extensions && (!page.sources.length || !!page.sourceError
                                             || (page.sourcesLoading && page.fromSources.length === 0)
                                             || (!page.sourcesLoading && page.fromSources.length === 0))
                loading: page.sourcesLoading && page.fromSources.length === 0
                task: page.sourceError
                empty: !page.sources.length || (!page.sourcesLoading && !page.sourceError && page.fromSources.length === 0)
                emptyTitle: !page.sources.length ? qsTr("No music extensions yet")
                          : page.query.length ? qsTr("No songs found") : qsTr("Search for a song")
                emptyText: !page.sources.length
                    ? qsTr("Add a repository of music extensions in Extensions, then install one. SCANCORE doesn't come with any.")
                    : page.query.length ? "" : qsTr("These extensions have no list of their own; search them above.")
                emptyIcon: "puzzle"
                onRetry: page.askSources()
            }
            ScButton {
                visible: page.extensions && !page.sources.length
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Open Extensions")
                iconName: "puzzle"
                onClicked: Router.openExtensions()
            }
            StateView {
                width: parent.width
                visible: !page.files && !page.extensions && (!Music.animeSongsAvailable || (page.query.length ? !!page.searchTask && !page.searchTask.ok : !!page.seasonTask && !page.seasonTask.ok)
                    || (page.query.length && page.searchTask && page.searchTask.ok && page.found.length === 0))
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
            // The album, or the extension, is the section above.
            showAnime: !page.files && !page.extensions
            onPlayed: MusicQueue.playList(page.shown, index)
        }
        // Retro and phones scroll with the finger; this keeps the wheel too.
        ScrollBar.vertical: ScScrollBar {}
    }
}
