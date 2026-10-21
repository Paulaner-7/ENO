import QtQuick
import QtQuick.Controls.Basic
import ScanCore

SettingsPane {
    id: pane
    title: qsTr("Reading")
    subtitle: qsTr("Comic and novel readers")
    iconName: "book-open"

    readonly property int tileWidth: Theme.compact ? Math.floor((width - 36) / 2) : 168
    readonly property var look: Theme.readingLook(App.settings.novelTheme)
    readonly property int sliderWidth: Theme.compact ? width - 80 : 220
    readonly property var railColours: [
        { key: "readerRailUnread", label: qsTr("Not read yet"), fallback: "#e4e4e7" },
        { key: "readerRailRead", label: qsTr("Read"), fallback: "#8b8b94" },
        { key: "readerRailCurrent", label: qsTr("Current page"), fallback: "#000000" }]

    // A value next to its slider.
    component Readout: Text {
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.textSm
        font.features: { "tnum": 1 }
        anchors.verticalCenter: parent.verticalCenter
    }

    SettingsGroup {
        title: qsTr("Comic reader")
        note: qsTr("While reading: M switches layout, Ctrl + scroll changes the strip width (or zooms a page), and F goes full screen. Each series can keep its own settings from the reader's settings button.")

        Flow {
            x: 12
            width: parent.width - 24
            topPadding: 12
            bottomPadding: 12
            spacing: 12
            ChoiceTile {
                width: pane.tileWidth
                label: qsTr("Vertical strip")
                detail: qsTr("Webtoons, manhwa")
                selected: App.settings.readerMode !== "paged"
                onClicked: App.setSetting("readerMode", "strip")
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Repeater {
                        model: 3
                        Rectangle { width: 34; height: 22; radius: 3; color: index === 1 ? Theme.borderStrong : Theme.surface3 }
                    }
                }
            }
            ChoiceTile {
                width: pane.tileWidth
                label: qsTr("Pages")
                detail: qsTr("Manga")
                selected: App.settings.readerMode === "paged"
                onClicked: App.setSetting("readerMode", "paged")
                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    Repeater {
                        model: 2
                        Rectangle { width: 30; height: 44; radius: 3; color: index === 0 ? Theme.borderStrong : Theme.surface3 }
                    }
                }
            }
        }
        SettingRow {
            title: qsTr("Page direction")
            description: qsTr("For the Pages layout.")
            Segmented {
                options: [{ value: "ltr", label: qsTr("Left to right") }, { value: "rtl", label: qsTr("Right to left") }]
                value: App.settings.readerDirection || "ltr"
                onPicked: (v) => App.setSetting("readerDirection", v)
            }
        }
        SettingRow {
            visible: App.platform !== "android"
            title: qsTr("Two pages side by side")
            description: qsTr("In the Pages layout, when the window is wider than it is tall. Covers and double-page spreads stay alone.")
            Toggle { checked: App.settings.readerSpread === "double"; onToggled: App.setSetting("readerSpread", checked ? "double" : "single") }
        }
        SettingRow {
            title: qsTr("Trim page borders")
            description: qsTr("Cuts the plain white or black margin around scanned pages in the Pages layout.")
            Toggle { checked: App.settings.readerCrop === true; onToggled: App.setSetting("readerCrop", checked) }
        }
        SettingRow {
            title: qsTr("Split wide pages")
            description: qsTr("Shows a two-page spread as its two halves, one after the other, in the Pages layout.")
            Toggle { checked: App.settings.readerSplitWide === true; onToggled: App.setSetting("readerSplitWide", checked) }
        }
        SettingRow {
            title: qsTr("Tap zones")
            description: qsTr("Where a tap turns the page back or forward, or shows the controls. The reader shows them when you change them.")
            Select {
                model: [{ value: "sides", label: qsTr("Left and right") }, { value: "lshape", label: qsTr("L shape") },
                        { value: "kindle", label: qsTr("Kindle") }, { value: "edge", label: qsTr("Edges") }, { value: "off", label: qsTr("Off") }]
                value: App.settings.readerTapZones || "sides"
                onPicked: (v) => App.setSetting("readerTapZones", v)
            }
        }
        SettingRow {
            visible: App.settings.readerTapZones !== "off"
            title: qsTr("Flip the tap zones")
            Select {
                model: [{ value: "none", label: qsTr("Not flipped") }, { value: "horizontal", label: qsTr("Left and right") },
                        { value: "vertical", label: qsTr("Top and bottom") }, { value: "both", label: qsTr("Both ways") }]
                value: App.settings.readerTapInvert || "none"
                onPicked: (v) => App.setSetting("readerTapInvert", v)
            }
        }
        SettingRow {
            visible: App.settings.readerTapZones !== "off"
            title: qsTr("Tap zones in the vertical strip")
            description: qsTr("A tap scrolls a screen up or down instead of showing the controls.")
            Toggle { checked: App.settings.readerStripTaps === true; onToggled: App.setSetting("readerStripTaps", checked) }
        }
        SettingRow {
            visible: App.platform === "android"
            title: qsTr("Volume keys turn pages")
            description: qsTr("Volume down goes forward, volume up back, while a comic is open.")
            Toggle { checked: App.settings.readerVolumeKeys === true; onToggled: App.setSetting("readerVolumeKeys", checked) }
        }
        SettingRow {
            title: qsTr("Strip width")
            description: qsTr("How wide the vertical strip is on large screens.")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 360; to: 2000; stepSize: 20; value: App.settings.readerWidth; onMoved: App.setSetting("readerWidth", value) }
                Readout { text: App.settings.readerWidth + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Gap between panels")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 0; to: 40; stepSize: 2; value: App.settings.readerGap || 0; onMoved: App.setSetting("readerGap", value) }
                Readout { text: (App.settings.readerGap || 0) + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Progress rail")
            description: qsTr("A thin line on the left edge showing how far through the chapter you are.")
            Toggle { checked: App.settings.readerProgressBar !== false; onToggled: App.setSetting("readerProgressBar", checked) }
        }
        SettingRow {
            visible: App.settings.readerProgressBar !== false
            title: qsTr("Progress rail colours")
            description: qsTr("Pages not read yet, pages read, and the page you are on.")
            Row {
                spacing: 10
                Repeater {
                    model: pane.railColours
                    AbstractButton {
                        id: swatch
                        required property var modelData
                        width: 36
                        height: 36
                        hoverEnabled: true
                        Accessible.name: modelData.label
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.label
                        onClicked: { railPicker.key = modelData.key; railPicker.title = modelData.label; railPicker.edit(App.settings[modelData.key]) }
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: App.settings[swatch.modelData.key] || "#000000"
                            border.width: 1
                            border.color: swatch.hovered ? Theme.text : Theme.borderStrong
                        }
                    }
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: pane.railColours.some(c => App.settings[c.key] !== c.fallback)
                    iconName: "rotate-ccw"
                    tip: qsTr("Reset colours")
                    Accessible.name: tip
                    onClicked: { for (const c of pane.railColours) App.setSetting(c.key, c.fallback) }
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Novel reader")

        // Live preview in the chosen type and reading theme.
        Item {
            width: parent.width
            height: preview.height + 24
            Rectangle {
                id: preview
                x: 12
                y: 12
                width: parent.width - 24
                height: sample.implicitHeight + 40
                radius: Theme.radiusMd
                color: pane.look.bg
                border.width: 1
                border.color: Theme.border
                Behavior on color { ColorAnimation { duration: Motion.normal } }
                Text {
                    id: sample
                    x: 20
                    y: 20
                    width: parent.width - 40
                    text: qsTr("The lamps of the old city came on one by one, and for a moment the whole street seemed to hold its breath. ")
                        + qsTr("She turned the page and kept reading.")
                    color: pane.look.text
                    font.family: App.settings.novelFont || Theme.fontSerif
                    font.weight: App.settings.novelWeight || Font.Normal
                    font.pixelSize: App.settings.novelSize || 19
                    lineHeight: App.settings.novelLineHeight || 1.5
                    horizontalAlignment: App.settings.novelJustify === true ? Text.AlignJustify : Text.AlignLeft
                    wrapMode: Text.Wrap
                }
            }
        }
        SettingRow {
            title: qsTr("Reading theme")
            Row {
                spacing: 8
                Repeater {
                    model: ["app", "paper", "sepia", "dark", "black", "custom"]
                    AbstractButton {
                        id: chip
                        required property string modelData
                        readonly property var colors: Theme.readingLook(modelData)
                        readonly property bool selected: (App.settings.novelTheme || "app") === modelData
                        width: Theme.compact ? 44 : 50
                        height: 50
                        hoverEnabled: true
                        onClicked: App.setSetting("novelTheme", modelData)
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: chip.colors.bg
                            border.width: chip.selected ? 2 : 1
                            border.color: chip.selected ? Theme.accent : chip.hovered ? Theme.borderStrong : Theme.border
                        }
                        contentItem: Column {
                            topPadding: 6
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Aa"
                                color: chip.colors.text
                                font.family: Theme.fontSerif
                                font.pixelSize: 15
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: chip.colors.label
                                color: chip.colors.muted
                                font.family: Theme.fontSans
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }
        }
        SettingRow {
            title: qsTr("Font")
            Segmented {
                options: [{ value: "Literata", label: qsTr("Serif") }, { value: "DM Sans", label: qsTr("Sans") }, { value: "Space Grotesk", label: qsTr("Grotesk") }]
                value: App.settings.novelFont
                onPicked: (v) => App.setSetting("novelFont", v)
            }
        }
        SettingRow {
            title: qsTr("Text size")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 13; to: 34; stepSize: 1; value: App.settings.novelSize; onMoved: App.setSetting("novelSize", value) }
                Readout { text: App.settings.novelSize + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Line spacing")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 1.2; to: 2.2; stepSize: 0.05; value: App.settings.novelLineHeight || 1.5; onMoved: App.setSetting("novelLineHeight", Math.round(value * 100) / 100) }
                Readout { text: Number(App.settings.novelLineHeight || 1.5).toFixed(2) }
            }
        }
        SettingRow {
            visible: !Theme.compact
            title: qsTr("Column width")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 480; to: 1100; stepSize: 20; value: App.settings.novelWidth || 720; onMoved: App.setSetting("novelWidth", value) }
                Readout { text: (App.settings.novelWidth || 720) + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Justify text")
            description: qsTr("Even right edges, like a printed book.")
            Toggle { checked: App.settings.novelJustify === true; onToggled: App.setSetting("novelJustify", checked) }
        }
        SettingRow {
            title: qsTr("Font weight")
            Segmented {
                options: [{ value: 300, label: qsTr("Light") }, { value: 400, label: qsTr("Regular") }, { value: 500, label: qsTr("Medium") }, { value: 600, label: qsTr("Bold") }]
                value: App.settings.novelWeight || 400
                onPicked: (v) => App.setSetting("novelWeight", v)
            }
        }
        SettingRow {
            title: qsTr("Paragraph spacing")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 0; to: 2.5; stepSize: 0.1; value: App.settings.novelParagraphSpacing === undefined ? 0.9 : App.settings.novelParagraphSpacing; onMoved: App.setSetting("novelParagraphSpacing", Math.round(value * 10) / 10) }
                Readout { text: Number(App.settings.novelParagraphSpacing === undefined ? 0.9 : App.settings.novelParagraphSpacing).toFixed(1) }
            }
        }
        SettingRow {
            title: qsTr("Side margins")
            description: qsTr("Space beside the text on narrow screens.")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 8; to: 120; stepSize: 2; value: App.settings.novelMargin === undefined ? 32 : App.settings.novelMargin; onMoved: App.setSetting("novelMargin", value) }
                Readout { text: (App.settings.novelMargin === undefined ? 32 : App.settings.novelMargin) + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Indent paragraphs")
            Toggle { checked: App.settings.novelIndent === true; onToggled: App.setSetting("novelIndent", checked) }
        }
        SettingRow {
            title: qsTr("Dim the page")
            description: qsTr("Darker than the screen goes, for reading in the dark.")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 0; to: 0.8; stepSize: 0.05; value: Number(App.settings.novelDim) || 0; onMoved: App.setSetting("novelDim", Math.round(value * 100) / 100) }
                Readout { text: Math.round((Number(App.settings.novelDim) || 0) * 100) + "%" }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Read aloud")
        SettingRow {
            visible: !Speech.available
            title: qsTr("No voices are installed on this device")
            description: qsTr("Add one in the system's text-to-speech settings.")
        }
        SettingRow {
            visible: Speech.available
            title: qsTr("Voice")
            Select {
                model: [{ value: "", label: qsTr("System voice") }].concat(Speech.voices.map(v => ({ value: v.id, label: v.name + " · " + v.language })))
                value: App.settings.ttsVoice || ""
                onPicked: (v) => App.setSetting("ttsVoice", v)
            }
        }
        SettingRow {
            visible: Speech.available
            title: qsTr("Voice speed")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 0.5; to: 3; stepSize: 0.05; value: Number(App.settings.ttsRate) || 1; onMoved: App.setSetting("ttsRate", Math.round(value * 100) / 100) }
                Readout { text: Number(App.settings.ttsRate || 1).toFixed(2) + "×" }
            }
        }
        SettingRow {
            visible: Speech.available
            title: qsTr("Voice pitch")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 0.5; to: 2; stepSize: 0.05; value: Number(App.settings.ttsPitch) || 1; onMoved: App.setSetting("ttsPitch", Math.round(value * 100) / 100) }
                Readout { text: Number(App.settings.ttsPitch || 1).toFixed(2) + "×" }
            }
        }
        SettingRow {
            visible: Speech.available
            title: qsTr("Go on to the next chapter")
            Toggle { checked: App.settings.ttsContinue !== false; onToggled: App.setSetting("ttsContinue", checked) }
        }
        SettingRow {
            visible: Speech.available
            title: qsTr("Keep the spoken paragraph on screen")
            Toggle { checked: App.settings.ttsFollow !== false; onToggled: App.setSetting("ttsFollow", checked) }
        }
    }

    ColorPicker {
        id: railPicker
        property string key
        swatches: ["#000000", "#18181b", "#3f3f46", "#71717a", "#8b8b94", "#a1a1aa", "#d4d4d8", "#e4e4e7",
                   "#ffffff", "#7c9cff", "#a78bfa", "#34d399", "#fb923c", "#fb7185"]
        onChanged: (c) => App.setSetting(key, String(c).slice(0, 7))
    }
}
