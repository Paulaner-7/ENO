import QtQuick
import Scancore

// Tilted columns of covers drifting up slowly: the welcome's backdrop.
// One animation moves the whole wall; the columns only sit apart.
Item {
    id: wall
    property WelcomeCoverSource covers
    property real coverWidth: Theme.compact ? 96 : 150
    property bool moving: true
    clip: true

    readonly property real gap: coverWidth * 0.14
    readonly property real coverHeight: coverWidth * 1.42
    readonly property int columns: Math.ceil(width / (coverWidth + gap)) + 3
    // Enough for the drift's half screen too.
    readonly property int rows: Math.ceil(height * 1.6 / (coverHeight + gap)) + 4

    Item {
        id: drift
        width: wall.width
        height: wall.height
        // Half a screen up over a minute, and back (a loop that jumped back
        // would show every seat changing cover at once).
        SequentialAnimation on y {
            running: wall.moving && wall.visible && !Motion.reduced
            loops: Animation.Infinite
            NumberAnimation { to: -wall.height * 0.5; duration: 60000; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0; duration: 60000; easing.type: Easing.InOutSine }
        }
        Row {
            x: (wall.width - width) / 2
            y: -wall.coverHeight * 0.8
            spacing: wall.gap
            rotation: -9
            Repeater {
                model: wall.columns
                Column {
                    id: column
                    required property int index
                    spacing: wall.gap
                    y: index % 2 ? -wall.coverHeight * 0.55 : 0
                    Repeater {
                        model: wall.rows
                        WelcomeArt {
                            required property int index
                            width: wall.coverWidth
                            height: wall.coverHeight
                            seed: column.index * 5 + index * 7
                            source: wall.covers ? wall.covers.pick(seed) : ""
                        }
                    }
                }
            }
        }
    }
}
