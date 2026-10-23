#include "MacGlass.h"
#include "Customize.h"

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#endif

namespace sc::macglass {

#ifdef Q_OS_MACOS
namespace {

id findGlassFilter(CALayer *layer) {
    if (!layer)
        return nil;
    if (layer.filters) {
        for (id filter in layer.filters) {
            if ([[filter description] isEqualToString:@"glassBackground"])
                return filter;
        }
    }
    for (CALayer *sub in layer.sublayers) {
        if (id found = findGlassFilter(sub))
            return found;
    }
    return nil;
}

double filterNumber(id filter, NSString *key, double fallback) {
    if (!filter)
        return fallback;
    @try {
        id val = [filter valueForKey:key];
        if ([val respondsToSelector:@selector(doubleValue)])
            return [val doubleValue];
    } @catch (NSException *) {
    }
    return fallback;
}

const void *kBackdropViewKey = &kBackdropViewKey;

} // namespace
#endif

QVariantMap opticalMetrics() {
    static const QVariantMap cached = []() -> QVariantMap {
        QVariantMap out{
            {QStringLiteral("nativeAvailable"), false},
            {QStringLiteral("interactiveAvailable"), false},
            {QStringLiteral("refractionAmount"), -30.0},
            {QStringLiteral("refractionHeight"), 15.0},
            {QStringLiteral("clearRefractionAmount"), -39.0},
            {QStringLiteral("clearRefractionHeight"), 20.0},
            {QStringLiteral("blurRadius"), 5.0},
            {QStringLiteral("clearBlurRadius"), 5.833333},
            {QStringLiteral("darkSaturation"), 1.3},
            {QStringLiteral("lightSaturation"), 1.2},
            {QStringLiteral("highlightAngle"), 1.5707963267948966},
            {QStringLiteral("highlightSpread"), 2.0943951023931953},
            {QStringLiteral("highlightAmount"), 0.5},
            {QStringLiteral("clearHighlightSpread"), 1.3089969389957472},
            {QStringLiteral("clearHighlightAmount"), 0.4},
            {QStringLiteral("bleedAmount"), 21.0},
        };
#ifdef Q_OS_MACOS
        @autoreleasepool {
            if (@available(macOS 26.0, *)) {
                Class glassCls = NSClassFromString(@"NSGlassEffectView");
                if (glassCls) {
                    out[QStringLiteral("nativeAvailable")] = true;
                    NSGlassEffectView *probe = [[glassCls alloc] initWithFrame:NSMakeRect(0, 0, 200, 60)];
                    probe.cornerRadius = 30.0;
                    probe.style = NSGlassEffectViewStyleRegular;
                    if (@available(macOS 27.0, *)) {
                        if ([probe respondsToSelector:@selector(setEffectIsInteractive:)]) {
                            probe.effectIsInteractive = YES;
                            out[QStringLiteral("interactiveAvailable")] = true;
                        }
                    }
                    [probe layoutSubtreeIfNeeded];
                    id regFilter = nil;
                    for (NSView *sub in probe.subviews) {
                        regFilter = findGlassFilter(sub.layer);
                        if (regFilter)
                            break;
                    }
                    if (regFilter) {
                        out[QStringLiteral("refractionAmount")] = filterNumber(regFilter, @"inputInnerRefractionAmount", -30.0);
                        out[QStringLiteral("refractionHeight")] = filterNumber(regFilter, @"inputInnerRefractionHeight", 15.0);
                        out[QStringLiteral("blurRadius")] = filterNumber(regFilter, @"inputBlurRadius", 5.0);
                        out[QStringLiteral("darkSaturation")] = filterNumber(regFilter, @"inputFaceColorMatrixSaturation", 1.3);
                        out[QStringLiteral("highlightAngle")] = filterNumber(regFilter, @"inputKeyFillHighlightAngle", 1.5707963267948966);
                        out[QStringLiteral("highlightSpread")] = filterNumber(regFilter, @"inputKeyFillHighlightSpread", 2.0943951023931953);
                        out[QStringLiteral("highlightAmount")] = filterNumber(regFilter, @"inputKeyFillHighlightAmount", 0.5);
                        out[QStringLiteral("bleedAmount")] = filterNumber(regFilter, @"inputBleedAmount", 21.0);
                    }
                    probe.style = NSGlassEffectViewStyleClear;
                    [probe layoutSubtreeIfNeeded];
                    id clearFilter = nil;
                    for (NSView *sub in probe.subviews) {
                        clearFilter = findGlassFilter(sub.layer);
                        if (clearFilter)
                            break;
                    }
                    if (clearFilter) {
                        out[QStringLiteral("clearRefractionAmount")] = filterNumber(clearFilter, @"inputInnerRefractionAmount", -39.0);
                        out[QStringLiteral("clearRefractionHeight")] = filterNumber(clearFilter, @"inputInnerRefractionHeight", 20.0);
                        out[QStringLiteral("clearBlurRadius")] = filterNumber(clearFilter, @"inputBlurRadius", 5.833333);
                        out[QStringLiteral("clearHighlightSpread")] = filterNumber(clearFilter, @"inputKeyFillHighlightSpread", 1.3089969389957472);
                        out[QStringLiteral("clearHighlightAmount")] = filterNumber(clearFilter, @"inputKeyFillHighlightAmount", 0.4);
                    }
                }
            }
        }
#endif
        return out;
    }();
    return cached;
}

void syncWindowGlass(QQuickWindow *window, bool dark, const QColor &tint, bool clearStyle) {
#ifdef Q_OS_MACOS
    if (!window)
        return;
    @autoreleasepool {
        NSView *qnsView = reinterpret_cast<NSView *>(window->winId());
        if (!qnsView)
            return;
        NSWindow *nsWindow = [qnsView window];
        if (!nsWindow)
            return;

        NSAppearance *appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
        if (nsWindow.appearance != appearance)
            nsWindow.appearance = appearance;

        if (!Customize::instance()->translucentWindow())
            return;

        nsWindow.opaque = NO;
        nsWindow.backgroundColor = [NSColor clearColor];

        if (@available(macOS 26.0, *)) {
            NSView *superview = [qnsView superview];
            if (!superview)
                return;

            NSGlassEffectView *glassView = static_cast<NSGlassEffectView *>(objc_getAssociatedObject(nsWindow, kBackdropViewKey));
            if (!glassView) {
                glassView = [[NSGlassEffectView alloc] initWithFrame:qnsView.frame];
                glassView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
                glassView.cornerRadius = 0.0;
                [superview addSubview:glassView positioned:NSWindowBelow relativeTo:qnsView];
                objc_setAssociatedObject(nsWindow, kBackdropViewKey, glassView, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            }
            glassView.frame = qnsView.frame;
            glassView.style = clearStyle ? NSGlassEffectViewStyleClear : NSGlassEffectViewStyleRegular;
            if (tint.isValid() && tint.alpha() > 0) {
                glassView.tintColor = [NSColor colorWithSRGBRed:tint.redF()
                                                          green:tint.greenF()
                                                           blue:tint.blueF()
                                                          alpha:tint.alphaF()];
            } else {
                glassView.tintColor = nil;
            }
        }
    }
#else
    Q_UNUSED(window)
    Q_UNUSED(dark)
    Q_UNUSED(tint)
    Q_UNUSED(clearStyle)
#endif
}

} // namespace sc::macglass
