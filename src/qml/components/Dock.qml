import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import ScanCore

// Navigation as a floating dock along the top or bottom of the window: every
// section as an icon that grows under the pointer, with its name above (or
// below) it, a highlight that slides to the current one, and search and
// settings at the ends. Over a page the dock is liquid glass: a live, blurred
// capture of what passes underneath, washed with the theme colour and edged
// with a light rim.
Item {
    id: dock
    property string current: "home"
    property string side: "bottom"
    // The page behind the dock, sampled for the glass. When unset, the dock
    // falls back to an opaque panel.
    property Item backdropItem: null
    signal navigate(string section)
    signal openPalette()

    readonly property var sections: [
        { key: "search", label: qsTr("Search"), icon: "search" },
        { key: "divider" },
        { key: "home", label: qsTr("Home"), icon: "house" },
        { key: "anime", label: qsTr("Anime"), icon: "tv" },
        { key: "comics", label: qsTr("Manga"), icon: "book-open" },
        { key: "novels", label: qsTr("Novels"), icon: "book-text" },
        { key: "library", label: qsTr("Library"), icon: "library" },
        { key: "history", label: qsTr("History"), icon: "history" },
        { key: "activity", label: qsTr("Activity"), icon: "activity" },
        { key: "downloads", label: qsTr("Downloads"), icon: "download" },
        { key: "notifications", label: qsTr("Notifications"), icon: "bell" },
        { key: "divider" },
        { key: "settings", label: qsTr("Settings"), icon: "settings" }
    ].filter(s => !s.key || Router.sectionAllowed(s.key))  // ENO: anime only

    readonly property bool glass: backdropItem !== null
    readonly property real radius: height / 2

    implicitWidth: row.implicitWidth + 16
    implicitHeight: 60

    // Native macOS 27 Liquid Glass: the page behind is refracted through a curved
    // meniscus bezel, blurred, vibrantly tinted and edged with a directional key-fill
    // specular rim that responds to pointer interaction.
    LiquidGlass {
        id: glassLayer
        anchors.fill: parent
        zTier: 1
        radius: dock.radius
        backdropItem: dock.backdropItem
        interactive: true
        trackPointer: true
    }

    // The dock floats over the page: clicks, wheel and buttons land on the
    // bar itself, never on what is underneath.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    // Sliding highlight behind the current section.
    Rectangle {
        id: highlight
        readonly property Item target: {
            for (let i = 0; i < buttons.count; ++i) {
                const it = buttons.itemAt(i)
                if (it && it.key === dock.current) return it
            }
            return null
        }
        visible: target !== null
        width: 46
        height: 46
        radius: 23
        y: (dock.height - height) / 2
        x: target ? row.x + target.x + (target.width - width) / 2 : 0
        color: Theme.alpha(Theme.text, Theme.dark ? 0.1 : 0.08)
        Behavior on x { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: dock.side === "top" ? -5 : parent.height + 2
            width: 16
            height: 3
            radius: 2
            color: Theme.accent
        }
    }

    Row {
        id: row
        x: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Repeater {
            id: buttons
            model: dock.sections
            delegate: Item {
                id: slot
                required property var modelData
                readonly property string key: modelData.key
                readonly property bool isHovered: button.hovered || nativeDockBtn.hovered
                width: key === "divider" ? 13 : 48
                height: 48

                NativeGlassButton {
                    id: nativeDockBtn
                    anchors.fill: parent
                    visible: !Theme.eink && isSupported
                    glass: false
                    zTier: 1
                    layout: slot.key === "divider" ? "divider" : "dock"
                    passthrough: slot.key === "divider"
                    active: dock.current === slot.key
                    indicatorSide: dock.side
                    accentColor: Theme.accent
                    iconName: slot.key === "divider" ? ""
                        : (slot.key === "notifications" && Notifications.unread > 0 ? "bell-dot" : (slot.modelData.icon || ""))
                    iconSize: 20
                    iconColor: slot.key === "divider" ? Theme.border : (dock.current === slot.key ? Theme.text : Theme.textMuted)
                    textColor: Theme.text
                    badgeCount: slot.key === "library" ? Library.unreadUpdates
                        : slot.key === "downloads" ? Downloads.pending
                        : slot.key === "notifications" ? Notifications.unread : 0
                    onClicked: if (slot.key !== "divider") (slot.key === "search" ? dock.openPalette() : dock.navigate(slot.key))
                }

                Rectangle {
                    visible: slot.key === "divider"
                    anchors.centerIn: parent
                    width: 1
                    height: 26
                    color: Theme.border
                }

                AbstractButton {
                    id: button
                    visible: slot.key !== "divider"
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: slot.key === "search" ? dock.openPalette() : dock.navigate(slot.key)
                    background: null
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: slot.modelData.icon || ""
                            size: 20
                            color: dock.current === slot.key ? Theme.text : button.hovered ? Theme.text : Theme.textMuted
                            scale: button.pressed ? 0.86 : button.hovered ? 1.18 : 1
                            Behavior on scale { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutBack } }
                        }
                        Rectangle {
                            readonly property int count: slot.key === "library" ? Library.unreadUpdates
                                : slot.key === "downloads" ? Downloads.pending : 0
                            visible: count > 0
                            x: parent.width / 2 + 7
                            y: 9
                            width: 8
                            height: 8
                            radius: 4
                            color: Theme.accent
                        }
                    }
                }

                // The section's name, floating clear of the dock.
                Item {
                    visible: opacity > 0 && slot.key !== "divider"
                    opacity: slot.isHovered ? 1 : 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: dock.side === "top" ? parent.height + 14 + (slot.isHovered ? 0 : -4) : -height - 14 + (slot.isHovered ? 0 : 4)
                    width: tipText.implicitWidth + 18
                    height: 26
                    Behavior on opacity { NumberAnimation { duration: Motion.fast } }
                    Behavior on y { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
                    LiquidGlass {
                        anchors.fill: parent
                        zTier: 1
                        radius: 8
                        backdropItem: dock.backdropItem
                        autoBackdrop: true
                        trackPointer: false
                        text: slot.modelData.label || ""
                        textColor: Theme.text
                        fontSize: Theme.textXs + 1
                        bold: true
                    }
                    Text {
                        id: tipText
                        anchors.centerIn: parent
                        text: slot.modelData.label || ""
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textXs + 1
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }
}
