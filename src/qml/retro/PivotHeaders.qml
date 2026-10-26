import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Windows Phone's pivot headers: big thin words in a row, the current one
// bright and first, the others dimmed after it in turn (they wrap around,
// as the pivot did). A drop-in for Segmented in the Retro mode:
// options [{value, label}], value, picked(value).
Item {
    id: pivot
    property var options: []
    property var value
    signal picked(var value)

    readonly property int currentIndex: Math.max(0, options.findIndex(o => o.value === value))
    // The options from the current one on, wrapping.
    readonly property var turned: options.length ? options.slice(currentIndex).concat(options.slice(0, currentIndex)) : []
    readonly property int size: Theme.compact ? 36 : 44

    function next() { if (options.length) picked(options[(currentIndex + 1) % options.length].value) }
    function previous() { if (options.length) picked(options[(currentIndex - 1 + options.length) % options.length].value) }

    implicitHeight: size * 1.3
    clip: true

    Row {
        id: words
        spacing: Math.round(pivot.size * 0.5)
        // Each change slides the row in from the right a little (an
        // animation, not an Animator: one that does not run while the
        // pivot is out of sight would leave the row shifted).
        property real nudge: 0
        x: nudge
        Repeater {
            model: pivot.turned
            AbstractButton {
                id: word
                required property var modelData
                required property int index
                height: pivot.height
                width: label.implicitWidth
                background: null
                Accessible.name: modelData.label
                onClicked: pivot.picked(modelData.value)
                contentItem: Text {
                    id: label
                    text: word.modelData.label
                    color: Theme.text
                    opacity: word.index === 0 ? 1 : 0.4
                    font.family: Theme.fontSans
                    font.pixelSize: pivot.size
                    font.weight: Font.Light
                    font.capitalization: Retro.upperCase ? Font.MixedCase : Font.AllLowercase
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    onCurrentIndexChanged: if (!Motion.reduced) slide.restart()
    NumberAnimation {
        id: slide
        target: words
        property: "nudge"
        from: pivot.size
        to: 0
        duration: Motion.normal
        easing.type: Easing.OutCubic
    }
}
