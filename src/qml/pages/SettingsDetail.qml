import QtQuick
import Scancore

// One Settings category on its own page (phones).
Item {
    id: page
    property var section: ({})
    property color tint: Theme.textMuted

    SmoothFlickable {
        anchors.fill: parent
        contentHeight: column.height + 48

        Column {
            id: column
            width: parent.width
            spacing: 10

            PageHeader {
                title: page.section.title || ""
                subtitle: page.section.subtitle || ""
                showBack: true
            }

            Loader {
                x: Theme.gutter
                width: column.width - 2 * Theme.gutter
                Component.onCompleted: setSource(Qt.resolvedUrl("../settings/" + page.section.file), { showHeader: false, tint: page.tint })
            }
        }
    }
}
