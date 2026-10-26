import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Sign in or create a scancore.onl account. Used by the welcome flow and
// Settings. Emits done() after a successful sign in or sign up.
Column {
    id: form
    property string mode: "signin"           // "signin" | "signup"
    property var task: null
    property string error: ""
    readonly property bool busy: task !== null && task.loading
    readonly property var checks: Account.passwordChecks(password.text)
    readonly property bool passwordOk: checks.every(c => c.passed)
    readonly property bool emailOk: /^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email.text.trim())
    // The site's sign-up rules (an access code on scancore.onl, or closed).
    property var signupRules: ({ open: true, accessCode: false, codeLength: 6, help: "" })
    property var rulesTask: null
    readonly property bool needsCode: mode === "signup" && signupRules.accessCode === true
    readonly property bool signupClosed: mode === "signup" && signupRules.open === false
    readonly property bool canSubmit: !busy && emailOk && (mode === "signin"
        ? password.text.length > 0
        : !signupClosed && passwordOk && username.text.trim().length >= 3 && !Account.usernameProblem(username.text)
          && (!needsCode || code.complete))
    signal done()

    function loadRules() {
        rulesTask = Account.signupOptions()
        rulesTask.finished.connect((rules) => form.signupRules = rules)
    }
    onModeChanged: if (mode === "signup") loadRules()
    Component.onCompleted: if (mode === "signup") loadRules()
    Connections {
        target: Account
        // The site can change in Settings → Sync while the form is open.
        function onChanged() { if (form.mode === "signup" && !Account.signedIn) form.loadRules() }
    }

    spacing: 16
    width: 380

    function submit() {
        if (!canSubmit) return
        error = ""
        task = mode === "signin" ? Account.signIn(email.text, password.text)
                                 : Account.signUp(email.text, username.text, password.text, needsCode ? code.text : "")
        task.finished.connect(() => {
            password.text = ""
            form.done()
        })
        task.failed.connect((message) => form.error = message)
    }

    Segmented {
        options: [{ value: "signin", label: qsTr("Sign in") }, { value: "signup", label: qsTr("Create account") }]
        value: form.mode
        onPicked: (v) => {
            form.mode = v
            form.error = ""
        }
    }

    FormField {
        id: email
        label: qsTr("Email")
        placeholder: "you@example.com"
        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
        problem: text.length > 3 && !form.emailOk ? qsTr("Enter a valid email address") : ""
        onAccepted: form.mode === "signup" ? username.focusInput() : password.focusInput()
    }

    FormField {
        id: username
        visible: form.mode === "signup"
        label: qsTr("Username")
        placeholder: qsTr("Shown on comments and reviews")
        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
        problem: Account.usernameProblem(text)
        hint: qsTr("3–32 letters, numbers or underscores")
        onAccepted: password.focusInput()
    }

    FormField {
        id: password
        label: qsTr("Password")
        placeholder: form.mode === "signin" ? qsTr("Your password") : qsTr("Choose a strong password")
        echoMode: TextInput.Password
        onAccepted: form.needsCode && !code.complete ? code.focusInput() : form.submit()
    }

    // Live password requirements while creating an account.
    Flow {
        visible: form.mode === "signup" && password.text.length > 0
        width: parent.width
        spacing: 6
        Repeater {
            model: form.checks
            Rectangle {
                required property var modelData
                height: 26
                width: checkRow.implicitWidth + 16
                radius: 13
                color: modelData.passed ? Theme.alpha(Theme.success, 0.14) : Theme.surface2
                Behavior on color { ColorAnimation { duration: Motion.normal } }
                Row {
                    id: checkRow
                    anchors.centerIn: parent
                    spacing: 5
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: modelData.passed ? "check" : "minus"
                        size: 12
                        color: modelData.passed ? Theme.success : Theme.textFaint
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: modelData.passed ? Theme.text : Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textXs + 1
                    }
                }
            }
        }
    }

    Column {
        visible: form.needsCode
        width: parent.width
        spacing: 6
        Text {
            text: qsTr("Access code")
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            font.weight: Font.Medium
        }
        CodeBoxes {
            id: code
            length: form.signupRules.codeLength || 6
            enabled: !form.busy
            onAccepted: form.submit()
        }
        Text {
            width: parent.width
            // scancore.onl's usual line is shown in the app's language.
            readonly property string standard: "Contact logesh@mediumdev.com for access"
            text: form.signupRules.help && form.signupRules.help !== standard ? form.signupRules.help
                                                                              : qsTr("Contact logesh@mediumdev.com for access")
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs + 1
            elide: Text.ElideRight
        }
    }

    Text {
        visible: form.signupClosed
        width: parent.width
        wrapMode: Text.Wrap
        text: qsTr("This server is not taking new accounts.")
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.textSm
    }

    Rectangle {
        visible: form.error.length > 0
        width: parent.width
        height: errorText.implicitHeight + 20
        radius: Theme.radiusMd
        color: Theme.alpha(Theme.danger, 0.12)
        Text {
            id: errorText
            anchors.fill: parent
            anchors.margins: 10
            text: form.error
            color: Theme.danger
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            wrapMode: Text.Wrap
        }
    }

    ScButton {
        width: parent.width
        variant: "primary"
        text: form.mode === "signin" ? qsTr("Sign in") : qsTr("Create account")
        loading: form.busy
        enabled: form.canSubmit
        onClicked: form.submit()
    }

    Text {
        width: parent.width
        wrapMode: Text.Wrap
        text: (Account.customSite ? qsTr("Your account lives on %1.") : qsTr("Your account lives on scancore.onl."))
            .arg(String(Account.site).replace(/^https?:\/\//, "")) + " " + qsTr("Your password is sent only there and is never stored by this app.")
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs + 1
    }
}
