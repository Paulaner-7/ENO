pragma Singleton
import QtQuick
QtObject {
    property var opened: []
    property var shell: null
    function keep(old, result) { return result }
    function takeRepositoryLink(text) { return false }
    function openSeries(item, cover) {
        opened = opened.concat([item])
        if (shell) shell.push("pages/ComicDetails.qml", { summary: item })
    }
    function openSource() { throw new Error("Source selection must stay inline") }
    function shelfTitle(key) { return key }
}
