import QtQuick
import ScanCore

// A row of chips that spans its parent. Wide windows wrap it onto more
// lines; phones keep it on one line that scrolls sideways, so a long list of
// sources does not push the page down.
Flickable {
    id: root
    default property alias chips: flow.data
    property real padding: Theme.gutter

    width: parent ? parent.width : 400
    height: flow.height
    // The Flow's own size, not its childrenRect (reading that from a binding
    // re-enters the layout: a binding loop).
    contentWidth: Theme.compact ? flow.implicitWidth + 2 * padding : width
    contentHeight: height
    interactive: Theme.compact
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    Flow {
        id: flow
        x: root.padding
        width: Theme.compact ? 100000 : root.width - 2 * root.padding
        spacing: 8
    }
}
