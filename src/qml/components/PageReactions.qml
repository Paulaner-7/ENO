import QtQuick
import Scancore

// Reactions on a chapter's pages, kept by the account's site (scancore.onl)
// under the chapter's discussion thread (as CommentsPanel finds it): the
// pages that have some (marks, for the page bar), how many of each kind a
// page has, and the reader's own. A site of one's own has them when its
// /api/capabilities says so: a copy of scancore.onl (discussions) finds the
// thread as the website does; a sync server (scancore-web, reactions) keys
// the chapter by its series' title and number.
QtObject {
    id: reactions
    property string media: "comic"
    property string title: ""
    property string comickHid: ""
    property string url: ""
    property string unit: ""

    readonly property bool signedIn: Account.signedIn && title !== ""
    // The site keeps reactions (scancore.onl, or a copy of it).
    property bool supported: !Account.customSite
    // The site keeps no discussions: the thread is made here.
    property bool ownThreads: false
    readonly property bool available: signedIn && supported
    // "u:<unitId>" or "c:<series>|<chapter>", once found.
    property string thread: ""
    property var pages: []
    property var mine: ({})
    readonly property var marks: pages.map(p => p.page)
    property var tasks: []

    // The six kinds, as the site names them, with what the reader shows.
    readonly property var kinds: [{ type: "LIKE", glyph: "👍" }, { type: "LOVE", glyph: "❤️" }, { type: "LAUGH", glyph: "😂" },
                                  { type: "WOW", glyph: "😮" }, { type: "SAD", glyph: "😢" }, { type: "ANGRY", glyph: "😡" }]

    function countsFor(page) {
        const found = pages.find(p => p.page === page)
        return found ? found.counts : ({})
    }
    function mineFor(page) { return mine[String(page)] || "" }

    function hold(task, done) {
        tasks = tasks.concat([task])
        task.finished.connect((result) => { reactions.tasks = reactions.tasks.filter(t => t !== task); done(result) })
        task.failed.connect(() => reactions.tasks = reactions.tasks.filter(t => t !== task))
    }
    function query(params) {
        return Object.keys(params).filter(k => params[k] !== undefined && params[k] !== null && params[k] !== "")
            .map(k => encodeURIComponent(k) + "=" + encodeURIComponent(params[k])).join("&")
    }

    function start() {
        thread = ""
        pages = []
        mine = ({})
        if (!signedIn) return
        if (Account.customSite && !supported) {
            hold(Account.request("GET", "/api/capabilities"), (caps) => {
                reactions.ownThreads = !caps.discussions && caps.reactions === true
                if (caps.discussions || caps.reactions === true) reactions.supported = true
            })
            return
        }
        if (ownThreads) {
            thread = "c:" + title + "|" + unit
            load()
            return
        }
        hold(Account.request("GET", "/api/discussions/resolve?" + query({ media: media, title: title.slice(0, 200), comickHid: comickHid,
                                                                          url: /^https?:/.test(url) ? url : "", unit: unit })), (found) => {
            reactions.thread = found.unitId ? "u:" + found.unitId
                             : "c:" + String(found.seriesName || reactions.title) + "|" + String(found.chapterName || reactions.unit)
            reactions.load()
        })
    }
    function load() {
        if (!thread) return
        hold(Account.request("GET", "/api/app/reactions?" + query({ thread: thread })), (result) => {
            reactions.pages = result.pages || []
            reactions.mine = result.mine || ({})
        })
    }
    // The same kind again takes it back.
    function react(page, type) {
        if (!thread) return
        const same = mineFor(page) === type
        hold(Account.request("POST", "/api/app/reactions", { thread: thread, page: page, type: same ? null : type }), () => reactions.load())
    }

    onTitleChanged: Qt.callLater(start)
    onUnitChanged: Qt.callLater(start)
    onSignedInChanged: Qt.callLater(start)
    onSupportedChanged: Qt.callLater(start)
}
