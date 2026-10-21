import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// A small single-line input for a settings row.
TextField {
    implicitHeight: 38
    leftPadding: 14
    rightPadding: 14
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.alpha(Theme.accent, 0.35)
    font.family: Theme.fontSans
    font.pixelSize: Theme.textMd
    selectByMouse: true
    verticalAlignment: TextInput.AlignVCenter
    background: Rectangle {
        radius: height / 2
        color: parent.activeFocus ? Theme.surface : Theme.surface2
        border.width: 1
        border.color: parent.activeFocus ? Theme.alpha(Theme.accent, 0.55) : Theme.border
    }
}
