import QtQuick
import QtQuick.Effects
import QtQuick.Window
import Scancore

// Native macOS 27 Liquid Glass surface (NSGlassEffectView / CASDFLayer + glassBackground).
// Captures the live page or artwork behind the item, refracts it through a curved
// meniscus bezel with chromatic dispersion, mixes a clearer refractive rim with a
// frosted vibrant interior, and edges the shape with a directional key-fill specular
// highlight that responds to pointer interaction (effectIsInteractive).
Item {
    id: glass

    // Live background item to sample and refract (defaults to Window.window.glassBackdrop
    // when autoBackdrop is true and the candidate does not contain this item).
    property Item backdropItem: null
    // Optional pre-cropped item/image (e.g. a card's cover or header banner) whose full
    // bounds already match this glass surface.
    property Item backdropTexture: null
    property bool autoBackdrop: true

    // Corner curvature (clamped to half of the shorter side in the shader).
    property real radius: Theme.radiusLg
    // "regular" (NSGlassEffectViewStyleRegular) or "clear" (NSGlassEffectViewStyleClear).
    property string glassStyle: "regular"
    readonly property bool isClear: glassStyle === "clear"

    // macOS 27 effectIsInteractive: responds visually to hover and press.
    property bool interactive: false
    property bool externalHovered: false
    property bool externalPressed: false
    property bool trackPointer: interactive

    // Native AppKit NSGlassEffectView configuration and optional foreground content.
    property bool nativeEnabled: true
    property int zTier: 0
    property string iconName: ""
    property int iconSize: 14
    property color iconColor: Theme.text
    property string text: ""
    property color textColor: Theme.text
    property int fontSize: Theme.textXs + 1
    property bool bold: false

    // Blur radius in pixels for the frosted interior.
    property int blurMax: isClear ? 44 : 76

    // Optional tint override; defaults to macOS 27 DarkAqua / Aqua glass wash.
    property color tintColor: Theme.dark
        ? Theme.alpha(Theme.custom ? Theme.palette.surface : "#121318", isClear ? 0.34 : 0.56)
        : Theme.alpha(Theme.custom ? Theme.palette.surface : "#ffffff", isClear ? 0.38 : 0.62)

    readonly property var metrics: Customize.liquidGlass || ({})
    readonly property bool pointerHovered: externalHovered || (trackPointer && hoverTracker.hovered)
    readonly property real interactiveLevel: !interactive ? 0.0 : (externalPressed ? 1.55 : pointerHovered ? 1.0 : 0.0)
    property real smoothInteractive: interactiveLevel
    Behavior on smoothInteractive { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }

    readonly property point pointerNorm: {
        if (trackPointer && hoverTracker.hovered && width > 0 && height > 0) {
            return Qt.point(
                Math.max(0, Math.min(1, hoverTracker.point.position.x / width)),
                Math.max(0, Math.min(1, hoverTracker.point.position.y / height))
            )
        }
        return Qt.point(0.35, 0.2)
    }

    function isSafeBackdrop(candidate, requireVisible) {
        if (!candidate || candidate === glass) return false
        if (requireVisible && !candidate.visible) return false
        if (candidate.status !== undefined && candidate.status !== Image.Ready) return false
        let p = glass.parent
        while (p) {
            if (p === candidate) return false
            p = p.parent
        }
        return true
    }

    readonly property Item resolvedBackdrop: {
        if (backdropTexture && isSafeBackdrop(backdropTexture, false)) return backdropTexture
        if (backdropItem && isSafeBackdrop(backdropItem, true)) return backdropItem
        if (autoBackdrop && Window.window && Window.window.glassBackdrop && isSafeBackdrop(Window.window.glassBackdrop, true))
            return Window.window.glassBackdrop
        return null
    }
    readonly property bool hasLiveBackdrop: !Theme.eink && resolvedBackdrop !== null && visible && width > 0 && height > 0
    readonly property bool cropsBackdrop: backdropTexture === null && resolvedBackdrop !== null

    HoverHandler {
        id: hoverTracker
        enabled: glass.trackPointer
    }

    // E-ink fallback: crisp high-contrast flat surface.
    Rectangle {
        anchors.fill: parent
        visible: Theme.eink
        radius: glass.radius
        color: Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
    }

    // Real native macOS 27 NSGlassEffectView / CASDFLayer plate.
    NativeGlassPlate {
        id: nativePlate
        anchors.fill: parent
        cornerRadius: glass.radius
        style: glass.glassStyle
        interactive: glass.interactive
        tintColor: glass.tintColor
        passthrough: true
        zTier: glass.zTier
        iconName: glass.iconName
        iconSize: glass.iconSize
        iconColor: glass.iconColor
        text: glass.text
        textColor: glass.textColor
        fontSize: glass.fontSize
        bold: glass.bold
        visible: glass.nativeEnabled && !Theme.eink && isSupported && glass.visible && glass.opacity > 0.01
    }

    // 1. Live sharp capture of the region behind this glass element (for curved bezel refraction).
    ShaderEffectSource {
        id: sharpCapture
        visible: false
        live: glass.hasLiveBackdrop && !nativePlate.visible
        sourceItem: glass.hasLiveBackdrop ? glass.resolvedBackdrop : null
        sourceRect: {
            if (!glass.hasLiveBackdrop || !glass.cropsBackdrop || !glass.resolvedBackdrop)
                return Qt.rect(0, 0, 0, 0)
            // Touch geometry dependencies so animations on ancestors update the crop rect at 120Hz.
            const _gx = glass.x, _gy = glass.y, _gw = glass.width, _gh = glass.height
            const _px = glass.parent ? glass.parent.x : 0
            const _py = glass.parent ? glass.parent.y : 0
            const _gpx = glass.parent && glass.parent.parent ? glass.parent.parent.x : 0
            const _gpy = glass.parent && glass.parent.parent ? glass.parent.parent.y : 0
            void(_gx); void(_gy); void(_gw); void(_gh); void(_px); void(_py); void(_gpx); void(_gpy)
            const p = glass.mapToItem(glass.resolvedBackdrop, 0, 0)
            return Qt.rect(p.x, p.y, glass.width, glass.height)
        }
    }

    // 2. Frosted Gaussian blur pass (sampled in the interior of the glass).
    Item {
        id: blurredCapture
        width: Math.max(1, glass.width)
        height: Math.max(1, glass.height)
        visible: false
        layer.enabled: glass.hasLiveBackdrop
        layer.smooth: true
        MultiEffect {
            anchors.fill: parent
            source: sharpCapture
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: glass.blurMax
        }
    }

    // 3. macOS 27 CASDFLayer + glassBackground optical shader (fallback when native plate is unavailable).
    ShaderEffect {
        id: glassShader
        anchors.fill: parent
        visible: !Theme.eink && !nativePlate.visible
        supportsAtlasTextures: true

        readonly property vector2d size: Qt.vector2d(width, height)
        readonly property vector2d pointer: Qt.vector2d(glass.pointerNorm.x, glass.pointerNorm.y)
        readonly property vector4d tint: Qt.vector4d(glass.tintColor.r, glass.tintColor.g, glass.tintColor.b, glass.tintColor.a)
        readonly property real radius: glass.radius
        readonly property real refractionAmount: glass.isClear
            ? (glass.metrics.clearRefractionAmount !== undefined ? glass.metrics.clearRefractionAmount : -39.0)
            : (glass.metrics.refractionAmount !== undefined ? glass.metrics.refractionAmount : -30.0)
        readonly property real refractionHeight: glass.isClear
            ? (glass.metrics.clearRefractionHeight !== undefined ? glass.metrics.clearRefractionHeight : 20.0)
            : (glass.metrics.refractionHeight !== undefined ? glass.metrics.refractionHeight : 15.0)
        readonly property real saturation: Theme.dark
            ? (glass.metrics.darkSaturation !== undefined ? glass.metrics.darkSaturation : 1.3)
            : (glass.metrics.lightSaturation !== undefined ? glass.metrics.lightSaturation : 1.2)
        readonly property real highlightAngle: glass.metrics.highlightAngle !== undefined ? glass.metrics.highlightAngle : 1.5707963
        readonly property real highlightSpread: glass.isClear
            ? (glass.metrics.clearHighlightSpread !== undefined ? glass.metrics.clearHighlightSpread : 1.3089969)
            : (glass.metrics.highlightSpread !== undefined ? glass.metrics.highlightSpread : 2.0943951)
        readonly property real highlightAmount: glass.isClear
            ? (glass.metrics.clearHighlightAmount !== undefined ? glass.metrics.clearHighlightAmount : 0.4)
            : (glass.metrics.highlightAmount !== undefined ? glass.metrics.highlightAmount : 0.5)
        readonly property real darkMode: Theme.dark ? 1.0 : 0.0
        readonly property real interactive: glass.smoothInteractive
        readonly property real clearStyle: glass.isClear ? 1.0 : 0.0
        readonly property real hasBackdrop: glass.hasLiveBackdrop ? 1.0 : 0.0
        readonly property var sharpSource: sharpCapture
        readonly property var blurredSource: blurredCapture

        fragmentShader: "qrc:/shaders/liquidglass.frag.qsb"
    }
}
