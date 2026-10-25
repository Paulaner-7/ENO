pragma Singleton
import QtQuick
QtObject {
    signal changed()
    function continueItems(media, limit) { const t = Comics.task(); t.succeed([]); return t }
    function refreshCover(id) {}
}
