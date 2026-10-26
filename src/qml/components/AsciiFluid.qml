import QtQuick
import QtQuick.Effects
import QtQuick.Window
import Scancore

// High-performance GPU-accelerated ASCII Fluid Animation background.
// Implements multidirectional 2-layer domain-warped viscous fluid math
// indexing into a 20-character monospace glyph atlas with neon-matrix color
// mapping, slight ambient blur (MultiEffect) and contrast dimming.
Item {
    id: root

    property real blur: Theme.asciiBlur
    property real dim: Theme.asciiDim
    property real speed: Theme.asciiSpeed
    property real fontSize: Theme.asciiFontSize
    property real vignette: 0.35
    property real time: 0
    // Native resolution rendering for crisp glyph definition without downscale pixelation
    layer.enabled: false

    readonly property real cellWidth: Math.max(4, fontSize * 0.62)
    readonly property real cellHeight: Math.max(6, fontSize * 0.95)
    // The window to wake each frame: the shader lives in a hidden layer (drawn
    // through the MultiEffect below), so Qt cannot tell the scene changed and
    // lets the render loop go idle when the window is not active.
    readonly property var hostWindow: Window.window

    // Base background matching the ASCII fluid void color
    Rectangle {
        anchors.fill: parent
        color: "#010301"
    }

    // Dynamic glyph atlas generated in C++ (high-res monospace symbols)
    Image {
        id: atlas
        source: "image://asciiglyphs/atlas"
        visible: false
        asynchronous: false
    }

    // Bundled fallback atlas
    Image {
        id: fallbackAtlas
        source: "qrc:/branding/asciiglyphs.png"
        visible: false
        asynchronous: false
    }

    readonly property var activeAtlas: atlas.status === Image.Ready ? atlas : fallbackAtlas

    // Raw shader fluid output layer
    Item {
        id: fluidLayer
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        visible: false

        ShaderEffect {
            id: shader
            anchors.fill: parent
            readonly property real time: root.time
            readonly property vector2d size: Qt.vector2d(Math.max(1, width), Math.max(1, height))
            readonly property vector2d cellSize: Qt.vector2d(root.cellWidth, root.cellHeight)
            readonly property real dim: root.dim
            readonly property real vignette: root.vignette
            readonly property var glyphAtlas: root.activeAtlas

            fragmentShader: "qrc:/shaders/asciifluid.frag.qsb"
        }
    }

    // Ambient blur filter softening symbol strokes into glowing fluid while keeping them crisp
    MultiEffect {
        id: blurredEffect
        anchors.fill: parent
        source: fluidLayer
        autoPaddingEnabled: false
        blurEnabled: true
        blur: Math.min(1, 0.22 + root.blur * 0.45)
        blurMax: 24
    }

    // Subtle dark wash ensuring light text and cards above the fluid never clash
    Rectangle {
        anchors.fill: parent
        color: Theme.alpha(Theme.base, 0.22)
    }

    // Perpetual frame-driven animation. FrameAnimation keeps a QML animation
    // running (so the macOS display link never sleeps) while time advances by
    // the real frame delta instead of a huge fixed duration: no int overflow
    // at low speeds, and no restart from zero when the window is covered,
    // minimised or hidden. 16800π is a common period of every time
    // coefficient in the shader (0.3–0.8), so wrapping there is seamless.
    readonly property real timeCycle: 16800 * Math.PI

    FrameAnimation {
        id: fluidAnim
        running: root.visible
        onTriggered: {
            // frameTime is in seconds.
            const advance = frameTime * 0.3 * Math.max(0.05, root.speed)
            root.time = (root.time + advance) % root.timeCycle
            // Keep the render loop awake even when nothing else changes.
            if (root.hostWindow)
                root.hostWindow.requestUpdate()
        }
    }
}
