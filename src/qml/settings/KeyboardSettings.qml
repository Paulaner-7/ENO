import QtQuick
import QtQuick.Controls.Basic
import Scancore

SettingsPane {
    id: pane
    title: qsTr("Keyboard")
    subtitle: qsTr("Shortcuts for the app, readers and player")
    iconName: "keyboard"

    // The key profile the readers' rows show and change ("" = everyday).
    property string profile: ""
    function profileFor(id) { return id.startsWith("reader.") ? profile : "" }

    // Gives `id` the keys `sequence`, taking them from an action that
    // already answers to them.
    function assign(id, sequence) {
        const other = Keymap.conflict(id, sequence, profile)
        if (other) {
            Keymap.set(other.id, Keymap.sequences(other.id, profileFor(other.id)).filter(s => !Customize.sameKeys(s, sequence)), profileFor(other.id))
            Router.toast(Customize.keyText(sequence) + qsTr(" moved from “") + other.label + "”")
        }
        Keymap.set(id, [sequence], profileFor(id))
    }

    SettingsGroup {
        title: qsTr("Shortcuts")
        note: qsTr("Click a shortcut, then press the new keys. Backspace leaves an action without one; Esc keeps what it had. ")
            + qsTr("Escape, the arrow keys and scrolling keys always work as they do.")
        SettingRow {
            iconName: "keyboard"
            title: qsTr("Every shortcut on one sheet")
            description: qsTr("Press ") + (Keymap.labels("shortcuts")[0] || "?") + qsTr(" anywhere to see them.")
            Row {
                spacing: 8
                ScButton { text: qsTr("Show"); onClicked: Router.shell.openShortcuts() }
                ScButton {
                    text: qsTr("Reset all")
                    variant: "ghost"
                    enabled: Object.keys(Keymap.changed).length > 0
                    onClicked: Keymap.resetAll()
                }
            }
        }
    }

    // Each scope's actions; the key profiles sit just above the readers'.
    Component {
        id: scopeGroup
        SettingsGroup {
            required property var modelData
            title: modelData.scope === "reader" && pane.profile ? modelData.title + " · " + pane.profile : modelData.title
            Repeater {
                model: Keymap.actions.filter(a => a.scope === modelData.scope)
                SettingRow {
                    id: row
                    required property var modelData
                    readonly property string profile: pane.profileFor(modelData.id)
                    title: modelData.label
                    description: Keymap.isDefault(modelData.id, profile) ? ""
                        : profile ? qsTr("Everyday: ") + (Keymap.labels(modelData.id).join(", ") || qsTr("none"))
                        : qsTr("Default: ") + (modelData.keys.length ? modelData.keys.map(k => Customize.keyText(k)).join(", ") : qsTr("none"))
                    Row {
                        spacing: 4
                        KeyRecorder {
                            labels: Keymap.labels(row.modelData.id, row.profile)
                            onCaptured: (sequence) => pane.assign(row.modelData.id, sequence)
                            onCleared: Keymap.set(row.modelData.id, [], row.profile)
                        }
                        IconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !Keymap.isDefault(row.modelData.id, row.profile)
                            iconName: "undo-2"
                            iconSize: 15
                            diameter: 32
                            tip: row.profile ? qsTr("Back to the everyday keys") : qsTr("Back to the default")
                            onClicked: Keymap.reset(row.modelData.id, row.profile)
                        }
                    }
                }
            }
        }
    }

    Repeater { model: Keymap.scopes.filter(s => s.scope === "app"); delegate: scopeGroup }
    SettingsGroup {
        title: qsTr("Key profiles")
        note: qsTr("Other keys for the readers, for the series you choose (in the reader's settings, under Keys): one-handed keys for webtoons, say. Keys a profile leaves alone stay as everyday.")
        SettingRow {
            iconName: "layers"
            title: qsTr("Readers' keys below")
            description: pane.profile ? qsTr("Changing the profile “%1”").arg(pane.profile) : qsTr("Changing the everyday keys")
            Row {
                spacing: 8
                Select {
                    Accessible.name: qsTr("Key profile")
                    model: [{ value: "", label: qsTr("Everyday keys") }].concat(Keymap.profileNames.map(n => ({ value: n, label: n })))
                    value: pane.profile
                    onPicked: (v) => pane.profile = v
                }
                ScButton {
                    visible: pane.profile !== ""
                    text: qsTr("Delete")
                    variant: "ghost"
                    onClicked: {
                        Keymap.removeProfile(pane.profile)
                        pane.profile = ""
                    }
                }
            }
        }
        SettingRow {
            iconName: "plus"
            title: qsTr("New profile")
            description: qsTr("Starts as the everyday keys")
            Row {
                spacing: 8
                InlineField {
                    id: profileName
                    width: 180
                    placeholderText: qsTr("Name, e.g. Webtoons")
                    onAccepted: addProfile.clicked()
                }
                ScButton {
                    id: addProfile
                    text: qsTr("Add")
                    enabled: profileName.text.trim() !== ""
                    onClicked: {
                        const name = profileName.text.trim()
                        if (!Keymap.addProfile(name)) {
                            Router.toast(qsTr("There is a profile with that name already."), "error")
                            return
                        }
                        profileName.text = ""
                        pane.profile = name
                    }
                }
            }
        }
    }
    Repeater { model: Keymap.scopes.filter(s => s.scope !== "app"); delegate: scopeGroup }
}
