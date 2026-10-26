import QtQuick
import QtQuick.Dialogs
import Scancore

// Chooses Mihon's storage folder (or the downloads folder in it). It joins
// the folders of Settings → Sources, and the chapters Mihon downloaded
// there count as downloads of the same chapters in the library, read where
// they are (Downloads.relinkMihon).
Item {
    id: picker
    // No toast when chapters were linked (the import dialog says it).
    property bool quiet: false
    readonly property var status: Downloads.mihon
    readonly property var roots: status.roots || ({})
    readonly property bool linked: Object.keys(roots).length > 0
    readonly property bool linking: status.linking === true
    // A folder chosen here that no link has reported on yet, and whether it
    // was on the list before (then it stays when it holds no downloads).
    property string pending: ""
    property bool pendingWasListed: false

    readonly property string summary: linking ? qsTr("Looking through Mihon's downloads…")
        : !linked ? qsTr("Choose Mihon's storage folder, or the downloads folder in it. Its chapters then open offline from your library, with your progress. Nothing is copied.")
        : qsTr("%1 chapters of %2 series open offline from Mihon's folder").arg(status.chapters || 0).arg(status.series || 0)
          + ((status.unmatchedSeries || 0) > 0 ? " · " + qsTr("%1 series not in your library are under On this device").arg(status.unmatchedSeries) : "")

    function open() { dialog.open() }

    FolderDialog {
        id: dialog
        title: qsTr("Mihon's storage folder")
        onAccepted: {
            const path = App.localPath(selectedFolder)
            const folders = Array.from(App.settings.localFolders || [])
            picker.pending = path
            picker.pendingWasListed = folders.indexOf(path) >= 0
            if (picker.pendingWasListed) Downloads.relinkMihon()
            else App.setSetting("localFolders", folders.concat([path]))
        }
    }
    Connections {
        target: Downloads
        function onMihonLinked(result) {
            if (!picker.pending || result.more) return
            const path = picker.pending
            picker.pending = ""
            if (result.error) {
                Router.toast(result.error, "error")
                return
            }
            if (!(result.roots || {})[path]) {
                Router.toast(qsTr("No Mihon downloads in that folder. Choose the folder Mihon keeps its data in (Mihon: More → Settings → Data and storage → Storage location), or the downloads folder inside it."), "error")
                if (!picker.pendingWasListed)
                    App.setSetting("localFolders", Array.from(App.settings.localFolders || []).filter(f => f !== path))
                return
            }
            if (picker.quiet) return
            if (result.chapters > 0)
                Router.toast(qsTr("%1 chapters of %2 series from Mihon now open offline").arg(result.chapters).arg(result.series), "success")
            else
                Router.toast(qsTr("Mihon's downloads are under On this device. Import a Mihon backup to link them to your library."), "info")
        }
    }
}
