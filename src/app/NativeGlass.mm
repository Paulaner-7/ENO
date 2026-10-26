#include "NativeGlass.h"

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QtQml/qqml.h>

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

namespace {

NSColor *nsColorFromQColor(const QColor &c, qreal alphaScale = 1.0) {
    if (!c.isValid())
        return [NSColor clearColor];
    return [NSColor colorWithRed:c.redF()
                           green:c.greenF()
                            blue:c.blueF()
                           alpha:std::clamp(c.alphaF() * alphaScale, 0.0, 1.0)];
}

NSString *sfSymbolForEnoName(const QString &name, bool active = false) {
    if (name.isEmpty())
        return @"";
    if (active) {
        static const QHash<QString, QString> s_activeMap = {
            {QStringLiteral("house"), QStringLiteral("house.fill")},
            {QStringLiteral("home"), QStringLiteral("house.fill")},
            {QStringLiteral("tv"), QStringLiteral("tv.fill")},
            {QStringLiteral("book"), QStringLiteral("book.fill")},
            {QStringLiteral("book-open"), QStringLiteral("book.fill")},
            {QStringLiteral("book-text"), QStringLiteral("text.book.closed.fill")},
            {QStringLiteral("library"), QStringLiteral("books.vertical.fill")},
            {QStringLiteral("download"), QStringLiteral("arrow.down.circle.fill")},
            {QStringLiteral("bell"), QStringLiteral("bell.fill")},
            {QStringLiteral("bell-dot"), QStringLiteral("bell.badge.fill")},
            {QStringLiteral("settings"), QStringLiteral("gearshape.fill")},
            {QStringLiteral("bookmark"), QStringLiteral("bookmark.fill")},
            {QStringLiteral("heart"), QStringLiteral("heart.fill")}
        };
        auto it = s_activeMap.constFind(name);
        if (it != s_activeMap.cend())
            return it.value().toNSString();
    }

    static const QHash<QString, QString> s_map = {
        {QStringLiteral("house"), QStringLiteral("house")},
        {QStringLiteral("home"), QStringLiteral("house")},
        {QStringLiteral("tv"), QStringLiteral("tv")},
        {QStringLiteral("tv-minimal"), QStringLiteral("tv")},
        {QStringLiteral("book"), QStringLiteral("book")},
        {QStringLiteral("book-open"), QStringLiteral("book")},
        {QStringLiteral("book-text"), QStringLiteral("text.book.closed")},
        {QStringLiteral("library"), QStringLiteral("books.vertical")},
        {QStringLiteral("history"), QStringLiteral("clock.arrow.circlepath")},
        {QStringLiteral("activity"), QStringLiteral("waveform.path.ecg")},
        {QStringLiteral("download"), QStringLiteral("arrow.down.circle")},
        {QStringLiteral("bell"), QStringLiteral("bell")},
        {QStringLiteral("bell-dot"), QStringLiteral("bell.badge.fill")},
        {QStringLiteral("settings"), QStringLiteral("gearshape")},
        {QStringLiteral("search"), QStringLiteral("magnifyingglass")},
        {QStringLiteral("panel-left"), QStringLiteral("sidebar.left")},
        {QStringLiteral("panel-right"), QStringLiteral("sidebar.right")},
        {QStringLiteral("arrow-left"), QStringLiteral("arrow.left")},
        {QStringLiteral("arrow-right"), QStringLiteral("arrow.right")},
        {QStringLiteral("arrow-up"), QStringLiteral("arrow.up")},
        {QStringLiteral("arrow-down"), QStringLiteral("arrow.down")},
        {QStringLiteral("arrow-up-down"), QStringLiteral("arrow.up.arrow.down")},
        {QStringLiteral("chevron-left"), QStringLiteral("chevron.left")},
        {QStringLiteral("chevron-right"), QStringLiteral("chevron.right")},
        {QStringLiteral("chevron-up"), QStringLiteral("chevron.up")},
        {QStringLiteral("chevron-down"), QStringLiteral("chevron.down")},
        {QStringLiteral("chevrons-left"), QStringLiteral("backward.end.fill")},
        {QStringLiteral("chevrons-right"), QStringLiteral("forward.end.fill")},
        {QStringLiteral("play"), QStringLiteral("play.fill")},
        {QStringLiteral("circle-play"), QStringLiteral("play.rectangle")},
        {QStringLiteral("pause"), QStringLiteral("pause.fill")},
        {QStringLiteral("skip-back"), QStringLiteral("backward.end.fill")},
        {QStringLiteral("skip-forward"), QStringLiteral("forward.end.fill")},
        {QStringLiteral("volume-2"), QStringLiteral("speaker.wave.2.fill")},
        {QStringLiteral("volume-x"), QStringLiteral("speaker.slash.fill")},
        {QStringLiteral("maximize"), QStringLiteral("arrow.up.left.and.arrow.down.right")},
        {QStringLiteral("minimize"), QStringLiteral("arrow.down.right.and.arrow.up.left")},
        {QStringLiteral("x"), QStringLiteral("xmark")},
        {QStringLiteral("close"), QStringLiteral("xmark")},
        {QStringLiteral("plus"), QStringLiteral("plus")},
        {QStringLiteral("minus"), QStringLiteral("minus")},
        {QStringLiteral("check"), QStringLiteral("checkmark")},
        {QStringLiteral("list"), QStringLiteral("list.bullet")},
        {QStringLiteral("list-checks"), QStringLiteral("checklist")},
        {QStringLiteral("list-filter"), QStringLiteral("line.3.horizontal.decrease")},
        {QStringLiteral("sliders-horizontal"), QStringLiteral("slider.horizontal.3")},
        {QStringLiteral("type"), QStringLiteral("textformat")},
        {QStringLiteral("bookmark"), QStringLiteral("bookmark")},
        {QStringLiteral("bookmark-check"), QStringLiteral("bookmark.fill")},
        {QStringLiteral("heart"), QStringLiteral("heart")},
        {QStringLiteral("star"), QStringLiteral("star.fill")},
        {QStringLiteral("share"), QStringLiteral("square.and.arrow.up")},
        {QStringLiteral("trash"), QStringLiteral("trash")},
        {QStringLiteral("rotate-ccw"), QStringLiteral("arrow.counterclockwise")},
        {QStringLiteral("refresh-cw"), QStringLiteral("arrow.clockwise")},
        {QStringLiteral("external-link"), QStringLiteral("arrow.up.right.square")},
        {QStringLiteral("compass"), QStringLiteral("safari")},
        {QStringLiteral("calendar"), QStringLiteral("calendar")},
        {QStringLiteral("puzzle"), QStringLiteral("puzzlepiece.extension")},
        {QStringLiteral("layout-dashboard"), QStringLiteral("square.grid.2x2")},
        {QStringLiteral("layout-grid"), QStringLiteral("square.grid.2x2")},
        {QStringLiteral("ellipsis"), QStringLiteral("ellipsis")},
        {QStringLiteral("more-vertical"), QStringLiteral("ellipsis")},
        {QStringLiteral("eye"), QStringLiteral("eye")},
        {QStringLiteral("eye-off"), QStringLiteral("eye.slash")},
        {QStringLiteral("venetian-mask"), QStringLiteral("eye.slash.fill")},
        {QStringLiteral("triangle-alert"), QStringLiteral("exclamationmark.triangle.fill")},
        {QStringLiteral("circle-alert"), QStringLiteral("exclamationmark.triangle.fill")},
        {QStringLiteral("info"), QStringLiteral("info.circle.fill")},
        {QStringLiteral("sparkles"), QStringLiteral("sparkles")},
        {QStringLiteral("sun"), QStringLiteral("sun.max.fill")},
        {QStringLiteral("moon"), QStringLiteral("moon.fill")},
        {QStringLiteral("lock"), QStringLiteral("lock.fill")},
        {QStringLiteral("cloud"), QStringLiteral("icloud.fill")},
        {QStringLiteral("keyboard"), QStringLiteral("keyboard")},
        {QStringLiteral("captions"), QStringLiteral("captions.bubble.fill")},
        {QStringLiteral("languages"), QStringLiteral("globe")},
        {QStringLiteral("gauge"), QStringLiteral("speedometer")},
        {QStringLiteral("server"), QStringLiteral("server.rack")},
        {QStringLiteral("message-circle"), QStringLiteral("bubble.left.fill")},
        {QStringLiteral("cast"), QStringLiteral("airplayvideo")},
        {QStringLiteral("move-vertical"), QStringLiteral("arrow.up.and.down")},
        {QStringLiteral("layers"), QStringLiteral("square.3.layers.3d")},
        {QStringLiteral("loader-circle"), QStringLiteral("arrow.triangle.2.circlepath")},
        {QStringLiteral("folder"), QStringLiteral("folder.fill")},
        {QStringLiteral("folder-open"), QStringLiteral("folder.fill")},
        {QStringLiteral("file-text"), QStringLiteral("doc.text.fill")},
        {QStringLiteral("globe"), QStringLiteral("globe")},
        {QStringLiteral("link"), QStringLiteral("link")},
        {QStringLiteral("copy"), QStringLiteral("doc.on.doc")},
        {QStringLiteral("pencil"), QStringLiteral("pencil")},
        {QStringLiteral("user"), QStringLiteral("person.fill")},
        {QStringLiteral("log-out"), QStringLiteral("rectangle.portrait.and.arrow.right")},
        {QStringLiteral("megaphone"), QStringLiteral("megaphone.fill")},
        {QStringLiteral("flame"), QStringLiteral("flame.fill")},
        {QStringLiteral("award"), QStringLiteral("rosette")},
        {QStringLiteral("zap"), QStringLiteral("bolt.fill")},
        {QStringLiteral("shield"), QStringLiteral("shield.fill")},
        {QStringLiteral("wifi-off"), QStringLiteral("wifi.slash")}
    };
    QString mapped = s_map.value(name, name);
    return mapped.toNSString();
}

NSImage *loadQrcNsImage(const QString &source) {
    if (source.isEmpty())
        return nil;
    static QHash<QString, void *> s_cache;
    auto it = s_cache.constFind(source);
    if (it != s_cache.cend())
        return (__bridge NSImage *)(it.value());

    QString path = source;
    if (path.startsWith(QLatin1String("qrc:/")))
        path = QLatin1String(":/") + path.mid(5);
    QImage qimg(path);
    if (qimg.isNull())
        return nil;
    qimg = qimg.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        const_cast<uchar *>(qimg.bits()), qimg.width(), qimg.height(), 8, qimg.bytesPerLine(), cs,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef cgImg = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    if (!cgImg)
        return nil;
    NSImage *nsImg = [[NSImage alloc] initWithCGImage:cgImg size:NSMakeSize(qimg.width(), qimg.height())];
    CGImageRelease(cgImg);
    s_cache.insert(source, (__bridge_retained void *)nsImg);
    return nsImg;
}

void drawSfSymbolInRect(NSString *symbolName, CGFloat pointSize, NSFontWeight weight, NSColor *tint, NSRect targetRect) {
    if (!symbolName || symbolName.length == 0 || targetRect.size.width <= 0 || targetRect.size.height <= 0)
        return;
    NSImage *base = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:nil];
    if (!base)
        return;
    NSImageSymbolConfiguration *cfg = [NSImageSymbolConfiguration configurationWithPointSize:pointSize weight:weight];
    NSImage *configured = [base imageWithSymbolConfiguration:cfg];
    if (!configured)
        configured = base;

    NSSize imgSize = configured.size;
    if (imgSize.width <= 0 || imgSize.height <= 0)
        imgSize = targetRect.size;
    CGFloat scale = std::min(targetRect.size.width / imgSize.width, targetRect.size.height / imgSize.height);
    scale = std::min<CGFloat>(scale, 1.35);
    CGFloat drawW = imgSize.width * scale;
    CGFloat drawH = imgSize.height * scale;
    NSRect drawRect = NSMakeRect(
        targetRect.origin.x + (targetRect.size.width - drawW) / 2.0,
        targetRect.origin.y + (targetRect.size.height - drawH) / 2.0,
        drawW, drawH);

    NSRect propRect = drawRect;
    CGImageRef cgImg = [configured CGImageForProposedRect:&propRect context:[NSGraphicsContext currentContext] hints:nil];
    if (!cgImg)
        return;

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    // Flip local coordinates for CGContextClipToMask when inside a flipped NSView
    if ([[NSGraphicsContext currentContext] isFlipped]) {
        CGContextTranslateCTM(ctx, 0, drawRect.origin.y * 2.0 + drawRect.size.height);
        CGContextScaleCTM(ctx, 1.0, -1.0);
    }
    CGContextClipToMask(ctx, NSRectToCGRect(drawRect), cgImg);
    [tint setFill];
    CGContextFillRect(ctx, NSRectToCGRect(drawRect));
    CGContextRestoreGState(ctx);
}

// 4-Tier Subview Ordering in QNSView:
// 7001: Page-level NativeGlassPlate (zTier == 0)
// 7002: Page-level NativeGlassButton (zTier == 0)
// 7003: Shell-chrome NativeGlassPlate (zTier >= 1: Sidebar, Dock, EditPageBar, Toasts, UpdateBanner, incognitoBadge)
// 7004: Shell-chrome NativeGlassButton (zTier >= 1: Sidebar/Dock/EditPageBar/Toast/Banner buttons & labels)
void insertNativeSubview(NSView *qnsView, NSView *child, NSInteger targetTag) {
    if (!qnsView || !child)
        return;
    if (child.superview == qnsView) {
        [child removeFromSuperview];
    }
    NSView *anchorAbove = nil;
    for (NSView *sub in qnsView.subviews) {
        if (sub != child && sub.tag > targetTag) {
            anchorAbove = sub;
            break;
        }
    }
    if (anchorAbove) {
        [qnsView addSubview:child positioned:NSWindowBelow relativeTo:anchorAbove];
    } else {
        [qnsView addSubview:child positioned:NSWindowAbove relativeTo:nil];
    }
}

struct PopupInfo {
    QRectF rect;
    bool modal = false;
};

// Collects every open Popup (menus, dialogs, dropdowns) but never ToolTips:
// tooltips appear after the pointer rests on a control and must not make the
// native glass vanish.
void collectOpenPopups(QQuickItem *item, QList<PopupInfo> &out, int depth = 0) {
    if (!item || depth > 10)
        return;
    const char *cls = item->metaObject()->className();
    if (qstrncmp(cls, "QQuickPopupItem", 15) == 0) {
        QObject *popup = item->parent();
        const bool tooltip = popup && popup->inherits("QQuickToolTip");
        if (!tooltip && item->isVisible() && item->opacity() > 0.05 && item->width() > 4 && item->height() > 4) {
            PopupInfo info;
            info.rect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
            info.modal = popup && popup->property("modal").toBool();
            out.append(info);
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children)
        collectOpenPopups(child, out, depth + 1);
}

QList<PopupInfo> openPopups(QQuickWindow *win) {
    static QQuickWindow *s_lastWin = nullptr;
    static qint64 s_lastCheckMs = 0;
    static QList<PopupInfo> s_last;
    if (!win || !win->contentItem())
        return {};
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (win == s_lastWin && (now - s_lastCheckMs) < 12)
        return s_last;
    s_lastWin = win;
    s_lastCheckMs = now;
    s_last.clear();
    QQuickItem *root = win->contentItem()->parentItem() ? win->contentItem()->parentItem() : win->contentItem();
    collectOpenPopups(root, s_last, 0);
    return s_last;
}

bool isInsidePopup(QQuickItem *item) {
    QQuickItem *p = item;
    while (p) {
        const char *cls = p->metaObject()->className();
        if (qstrncmp(cls, "QQuickPopupItem", 15) == 0 || qstrncmp(cls, "QQuickOverlay", 13) == 0)
            return true;
        p = p->parentItem();
    }
    return false;
}

bool computeEffectiveVisibility(QQuickItem *item, QRectF *outSceneRect, qreal *outOpacity = nullptr) {
    if (!item || !item->window())
        return false;
    if (!item->isVisible() || item->opacity() <= 0.01 || item->width() <= 1.0 || item->height() <= 1.0)
        return false;

    QRectF sceneRect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    QRectF clipRect(0, 0, item->window()->width(), item->window()->height());
    qreal effOpacity = item->opacity();

    QQuickItem *p = item->parentItem();
    while (p) {
        if (!p->isVisible() || p->opacity() <= 0.01)
            return false;
        effOpacity *= p->opacity();
        if (p->clip() && p->width() > 0 && p->height() > 0) {
            clipRect = clipRect.intersected(p->mapRectToScene(QRectF(0, 0, p->width(), p->height())));
        }
        p = p->parentItem();
    }
    if (effOpacity <= 0.01)
        return false;

    // Native views sit above all QML, so they step aside only where an open
    // Popup would be covered: everywhere for a modal one, otherwise just
    // where the two overlap.
    if (!isInsidePopup(item)) {
        for (const PopupInfo &pi : openPopups(item->window())) {
            if (pi.modal || pi.rect.intersects(sceneRect))
                return false;
        }
    }

    QRectF visiblePart = sceneRect.intersected(clipRect);
    if (visiblePart.width() < sceneRect.width() * 0.5 || visiblePart.height() < sceneRect.height() * 0.5)
        return false;

    if (outSceneRect)
        *outSceneRect = sceneRect;
    if (outOpacity)
        *outOpacity = effOpacity;
    return true;
}

} // namespace

// -----------------------------------------------------------------------------
// Objective-C Views for NativeGlassPlate & NativeGlassButton
// -----------------------------------------------------------------------------

@interface PlateContentView : NSView
@property (nonatomic, copy) NSString *iconName;
@property (nonatomic, assign) int iconSize;
@property (nonatomic, strong) NSColor *iconColor;
@property (nonatomic, copy) NSString *text;
@property (nonatomic, strong) NSColor *textColor;
@property (nonatomic, assign) int fontSize;
@property (nonatomic, assign) BOOL bold;
@property (nonatomic, copy) NSString *textAlign;
@property (nonatomic, assign) int textRightPadding;
@end

@implementation PlateContentView
- (BOOL)isFlipped { return YES; }
- (NSView *)hitTest:(NSPoint)point { return nil; }
- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    NSRect b = self.bounds;
    if (b.size.width <= 1 || b.size.height <= 1)
        return;

    BOOL hasIcon = (self.iconName && self.iconName.length > 0);
    BOOL hasText = (self.text && self.text.length > 0);
    if (!hasIcon && !hasText)
        return;

    NSFont *font = [NSFont systemFontOfSize:std::max(9, self.fontSize)
                                     weight:(self.bold ? NSFontWeightSemibold : NSFontWeightMedium)];
    NSDictionary *attrs = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: (self.textColor ?: [NSColor whiteColor])
    };
    NSSize textSize = hasText ? [self.text sizeWithAttributes:attrs] : NSZeroSize;
    CGFloat iconW = hasIcon ? self.iconSize : 0.0;
    CGFloat spacing = (hasIcon && hasText) ? 5.0 : 0.0;
    CGFloat totalW = iconW + spacing + textSize.width;
    const BOOL right = hasText && [self.textAlign isEqualToString:@"right"];
    CGFloat startX = right ? b.size.width - totalW - self.textRightPadding
                           : (b.size.width - totalW) / 2.0;
    startX = std::max<CGFloat>(4.0, startX);

    if (hasIcon) {
        NSRect iconRect = NSMakeRect(startX, (b.size.height - iconW) / 2.0, iconW, iconW);
        drawSfSymbolInRect(self.iconName, self.iconSize * 0.85, self.bold ? NSFontWeightSemibold : NSFontWeightMedium,
                           self.iconColor ?: [NSColor whiteColor], iconRect);
        startX += iconW + spacing;
    }
    if (hasText) {
        NSRect textRect = NSMakeRect(startX, (b.size.height - textSize.height) / 2.0,
                                     std::min<CGFloat>(textSize.width + 2.0, b.size.width - startX - 4.0), textSize.height);
        [self.text drawInRect:textRect withAttributes:attrs];
    }
}
@end

@interface PassthroughGlassView : NSGlassEffectView
@property (nonatomic, assign) BOOL passthrough;
@property (nonatomic, assign) NSInteger customTag;
@property (nonatomic, strong) PlateContentView *plateContent;
@end

@implementation PassthroughGlassView
- (NSInteger)tag { return self.customTag; }
- (NSView *)hitTest:(NSPoint)point {
    if (self.passthrough)
        return nil;
    return [super hitTest:point];
}
- (void)scrollWheel:(NSEvent *)event {
    [self.superview scrollWheel:event];
}
- (void)layout {
    [super layout];
    if (self.plateContent) {
        self.plateContent.frame = self.bounds;
    }
}
@end

@interface ButtonForegroundView : NSView
@property (nonatomic, assign) sc::NativeGlassButton *owner;
@property (nonatomic, assign) NSInteger customTag;
@property (nonatomic, assign) BOOL hovered;
@property (nonatomic, assign) BOOL pressed;
@property (nonatomic, strong) NSTrackingArea *trackingArea;
@end

@implementation ButtonForegroundView
- (BOOL)isFlipped { return YES; }
- (NSInteger)tag { return self.customTag; }

- (NSView *)hitTest:(NSPoint)point {
    if (!self.owner || self.owner->passthrough() || !self.owner->isEnabled())
        return nil;
    return NSPointInRect(point, self.bounds) ? self : nil;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (self.trackingArea) {
        [self removeTrackingArea:self.trackingArea];
    }
    NSTrackingAreaOptions opts = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect;
    self.trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds options:opts owner:self userInfo:nil];
    [self addTrackingArea:self.trackingArea];
}

- (void)resetCursorRects {
    [super resetCursorRects];
    if (self.owner && !self.owner->passthrough() && self.owner->isEnabled()) {
        [self addCursorRect:self.bounds cursor:[NSCursor pointingHandCursor]];
    }
}

- (void)mouseEntered:(NSEvent *)event {
    self.hovered = YES;
    if (self.owner)
        self.owner->updateHovered(true);
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)event {
    self.hovered = NO;
    self.pressed = NO;
    if (self.owner) {
        self.owner->updateHovered(false);
        self.owner->updatePressed(false);
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)event {
    if (!self.owner || self.owner->passthrough() || !self.owner->isEnabled()) {
        [super mouseDown:event];
        return;
    }
    self.pressed = YES;
    self.owner->updatePressed(true);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    if (!self.owner || self.owner->passthrough() || !self.owner->isEnabled()) {
        [super mouseUp:event];
        return;
    }
    BOOL wasPressed = self.pressed;
    self.pressed = NO;
    self.owner->updatePressed(false);
    [self setNeedsDisplay:YES];
    NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    if (wasPressed && NSPointInRect(local, self.bounds)) {
        self.owner->emitClicked();
    }
}

- (void)scrollWheel:(NSEvent *)event {
    if (self.superview)
        [self.superview scrollWheel:event];
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    if (!self.owner)
        return;

    NSRect b = self.bounds;
    if (b.size.width <= 1 || b.size.height <= 1)
        return;

    const QString layout = self.owner->layout();

    // 1. Divider layout
    if (layout == QLatin1String("divider")) {
        [nsColorFromQColor(self.owner->iconColor()) setFill];
        if (b.size.width > b.size.height) {
            NSRect line = NSMakeRect(20.0, std::floor(b.size.height / 2.0), std::max<CGFloat>(1.0, b.size.width - 40.0), 1.0);
            NSRectFill(line);
        } else {
            NSRect line = NSMakeRect(std::floor(b.size.width / 2.0), (b.size.height - 26.0) / 2.0, 1.0, 26.0);
            NSRectFill(line);
        }
        return;
    }

    // 2. Brand logo layout (Sidebar header)
    if (layout == QLatin1String("brand")) {
        NSImage *img = loadQrcNsImage(self.owner->imageSource());
        if (!img || img.size.height <= 0)
            return;
        CGFloat aspect = img.size.width / img.size.height;
        if (self.owner->collapsed()) {
            CGFloat h = 32.0;
            CGFloat w = std::round(h * aspect);
            NSRect r = NSMakeRect((b.size.width - w) / 2.0, (b.size.height - h) / 2.0, w, h);
            [img drawInRect:r fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0 respectFlipped:YES hints:nil];
        } else {
            CGFloat h = 36.0;
            CGFloat w = std::round(h * aspect);
            NSRect r = NSMakeRect(20.0, (b.size.height - h) / 2.0, w, h);
            [img drawInRect:r fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0 respectFlipped:YES hints:nil];
        }
        return;
    }

    // 3. Sidebar item layout (Search bar, Nav items, Settings, Collapse)
    if (layout == QLatin1String("sidebar")) {
        const bool collapsed = self.owner->collapsed();
        const bool active = self.owner->active();
        const bool isSearch = self.owner->isSearch();
        const CGFloat radius = 10.0;

        if (isSearch) {
            NSBezierPath *bg = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.5, 0.5) xRadius:radius yRadius:radius];
            [nsColorFromQColor(self.owner->fillColor(), self.hovered ? 1.15 : 1.0) setFill];
            [bg fill];
            if (self.owner->borderColor().alpha() > 0) {
                [nsColorFromQColor(self.owner->borderColor()) setStroke];
                bg.lineWidth = 1.0;
                [bg stroke];
            }
        } else if (active) {
            NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:b xRadius:radius yRadius:radius];
            NSColor *pillColor = [nsColorFromQColor(self.owner->iconColor()) colorWithAlphaComponent:0.11];
            [pillColor setFill];
            [pill fill];

            // Accent indicator bar
            [nsColorFromQColor(self.owner->accentColor()) setFill];
            CGFloat barX = (self.owner->indicatorSide() == QLatin1String("right")) ? (b.size.width - 3.0) : 0.0;
            NSBezierPath *bar = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(barX, (b.size.height - 18.0) / 2.0, 3.0, 18.0) xRadius:1.5 yRadius:1.5];
            [bar fill];
        } else if (self.hovered) {
            NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:b xRadius:radius yRadius:radius];
            [[nsColorFromQColor(self.owner->iconColor()) colorWithAlphaComponent:0.06] setFill];
            [pill fill];
        }

        // Apple SF Symbol
        NSString *sym = sfSymbolForEnoName(self.owner->iconName(), active);
        CGFloat iconBox = self.owner->iconSize() + 4.0;
        CGFloat iconX = collapsed ? (b.size.width - iconBox) / 2.0 : (21.0 - iconBox / 2.0);
        CGFloat scale = self.pressed ? 0.88 : 1.0;
        NSRect iconRect = NSMakeRect(iconX, (b.size.height - iconBox) / 2.0, iconBox, iconBox);
        drawSfSymbolInRect(sym, self.owner->iconSize() * 0.86 * scale, active ? NSFontWeightSemibold : NSFontWeightMedium,
                           nsColorFromQColor(self.owner->iconColor()), iconRect);

        // Right badge count or shortcut chip
        CGFloat rightReserved = 8.0;
        if (self.owner->badgeCount() > 0) {
            if (collapsed) {
                NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(b.size.width / 2.0 + 5.0, 5.0, 8.0, 8.0)];
                [nsColorFromQColor(self.owner->accentColor()) setFill];
                [dot fill];
            } else {
                NSString *cntStr = (self.owner->badgeCount() > 99) ? @"99+" : [NSString stringWithFormat:@"%d", self.owner->badgeCount()];
                NSDictionary *cntAttrs = @{
                    NSFontAttributeName: [NSFont systemFontOfSize:11.0 weight:NSFontWeightBold],
                    NSForegroundColorAttributeName: nsColorFromQColor(self.owner->badgeTextColor())
                };
                NSSize cs = [cntStr sizeWithAttributes:cntAttrs];
                CGFloat bw = std::max<CGFloat>(20.0, cs.width + 12.0);
                CGFloat bh = 20.0;
                NSRect br = NSMakeRect(b.size.width - bw - 8.0, (b.size.height - bh) / 2.0, bw, bh);
                NSBezierPath *bp = [NSBezierPath bezierPathWithRoundedRect:br xRadius:bh / 2.0 yRadius:bh / 2.0];
                [nsColorFromQColor(self.owner->accentColor()) setFill];
                [bp fill];
                [cntStr drawInRect:NSMakeRect(br.origin.x + (bw - cs.width) / 2.0, br.origin.y + (bh - cs.height) / 2.0, cs.width + 1.0, cs.height)
                    withAttributes:cntAttrs];
                rightReserved = bw + 14.0;
            }
        } else if (!collapsed && !self.owner->badgeText().isEmpty()) {
            NSString *chipStr = self.owner->badgeText().toNSString();
            NSDictionary *chipAttrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium],
                NSForegroundColorAttributeName: nsColorFromQColor(self.owner->textColor())
            };
            NSSize cs = [chipStr sizeWithAttributes:chipAttrs];
            CGFloat cw = cs.width + 10.0;
            CGFloat ch = 20.0;
            NSRect cr = NSMakeRect(b.size.width - cw - 8.0, (b.size.height - ch) / 2.0, cw, ch);
            NSBezierPath *cp = [NSBezierPath bezierPathWithRoundedRect:cr xRadius:5.0 yRadius:5.0];
            [[nsColorFromQColor(self.owner->iconColor()) colorWithAlphaComponent:0.12] setFill];
            [cp fill];
            [chipStr drawInRect:NSMakeRect(cr.origin.x + (cw - cs.width) / 2.0, cr.origin.y + (ch - cs.height) / 2.0, cs.width + 1.0, cs.height)
                 withAttributes:chipAttrs];
            rightReserved = cw + 14.0;
        }

        // Label text
        if (!collapsed && !self.owner->text().isEmpty()) {
            NSString *lbl = self.owner->text().toNSString();
            NSMutableParagraphStyle *pstyle = [[NSMutableParagraphStyle alloc] init];
            pstyle.lineBreakMode = NSLineBreakByTruncatingTail;
            NSDictionary *attrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:std::max(11, self.owner->fontSize())
                                                       weight:(active ? NSFontWeightSemibold : NSFontWeightMedium)],
                NSForegroundColorAttributeName: nsColorFromQColor(self.owner->textColor()),
                NSParagraphStyleAttributeName: pstyle
            };
            NSSize ts = [lbl sizeWithAttributes:attrs];
            CGFloat textX = isSearch ? 38.0 : 44.0;
            CGFloat maxW = std::max<CGFloat>(0.0, b.size.width - textX - rightReserved);
            if (maxW > 4.0) {
                [lbl drawInRect:NSMakeRect(textX, (b.size.height - ts.height) / 2.0, maxW, ts.height) withAttributes:attrs];
            }
        }
        return;
    }

    // 4. Dock item layout
    if (layout == QLatin1String("dock")) {
        // The current section is told by the filled symbol alone (sfSymbolForEnoName
        // swaps in the .fill variant); there is no pill behind it and no accent
        // bar, and hover shows only through the icon growing.
        const bool active = self.owner->active();

        NSString *sym = sfSymbolForEnoName(self.owner->iconName(), active);
        CGFloat scale = self.pressed ? 0.86 : (self.hovered ? 1.15 : 1.0);
        CGFloat box = 26.0 * scale;
        NSRect iconRect = NSMakeRect((b.size.width - box) / 2.0, (b.size.height - box) / 2.0, box, box);
        NSColor *tint = (active || self.hovered) ? nsColorFromQColor(self.owner->textColor()) : nsColorFromQColor(self.owner->iconColor());
        drawSfSymbolInRect(sym, self.owner->iconSize() * 0.88 * scale, active ? NSFontWeightSemibold : NSFontWeightMedium, tint, iconRect);

        if (self.owner->badgeCount() > 0) {
            NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(b.size.width / 2.0 + 7.0, 9.0, 8.0, 8.0)];
            [nsColorFromQColor(self.owner->accentColor()) setFill];
            [dot fill];
        }
        return;
    }

    // 5. Toast / UpdateBanner foreground layout
    if (layout == QLatin1String("toast")) {
        const bool hasSubtitle = !self.owner->badgeText().isEmpty();
        const CGFloat badgeDim = hasSubtitle ? 40.0 : 28.0;
        const CGFloat badgeRadius = hasSubtitle ? 12.0 : 14.0;
        const CGFloat badgeX = hasSubtitle ? 16.0 : 12.0;
        NSRect badgeRect = NSMakeRect(badgeX, (b.size.height - badgeDim) / 2.0, badgeDim, badgeDim);
        NSBezierPath *badgePath = [NSBezierPath bezierPathWithRoundedRect:badgeRect xRadius:badgeRadius yRadius:badgeRadius];
        [nsColorFromQColor(self.owner->accentColor()) setFill];
        [badgePath fill];

        NSString *sym = sfSymbolForEnoName(self.owner->iconName(), true);
        drawSfSymbolInRect(sym, hasSubtitle ? 16.0 : 13.5, NSFontWeightSemibold, nsColorFromQColor(self.owner->iconColor()), badgeRect);

        CGFloat textX = badgeX + badgeDim + 12.0;
        CGFloat maxW = std::max<CGFloat>(20.0, b.size.width - textX - self.owner->rightInset());
        NSMutableParagraphStyle *pstyle = [[NSMutableParagraphStyle alloc] init];
        pstyle.lineBreakMode = NSLineBreakByWordWrapping;

        if (hasSubtitle) {
            NSMutableParagraphStyle *elideStyle = [[NSMutableParagraphStyle alloc] init];
            elideStyle.lineBreakMode = NSLineBreakByTruncatingTail;
            NSDictionary *titleAttrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:std::max(11, self.owner->fontSize()) weight:NSFontWeightSemibold],
                NSForegroundColorAttributeName: nsColorFromQColor(self.owner->textColor()),
                NSParagraphStyleAttributeName: elideStyle
            };
            NSDictionary *subAttrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:std::max(10, self.owner->fontSize() - 2) weight:NSFontWeightRegular],
                NSForegroundColorAttributeName: nsColorFromQColor(self.owner->badgeTextColor()),
                NSParagraphStyleAttributeName: elideStyle
            };
            NSString *tStr = self.owner->text().toNSString();
            NSString *sStr = self.owner->badgeText().toNSString();
            NSSize ts = [tStr sizeWithAttributes:titleAttrs];
            NSSize ss = [sStr sizeWithAttributes:subAttrs];
            CGFloat totalH = ts.height + 2.0 + ss.height;
            CGFloat startY = (b.size.height - totalH) / 2.0;
            [tStr drawInRect:NSMakeRect(textX, startY, maxW, ts.height) withAttributes:titleAttrs];
            [sStr drawInRect:NSMakeRect(textX, startY + ts.height + 2.0, maxW, ss.height) withAttributes:subAttrs];
        } else {
            NSDictionary *msgAttrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:std::max(11, self.owner->fontSize()) weight:NSFontWeightMedium],
                NSForegroundColorAttributeName: nsColorFromQColor(self.owner->textColor()),
                NSParagraphStyleAttributeName: pstyle
            };
            NSString *mStr = self.owner->text().toNSString();
            NSRect bound = [mStr boundingRectWithSize:NSMakeSize(maxW, b.size.height - 12.0)
                                              options:NSStringDrawingUsesLineFragmentOrigin
                                           attributes:msgAttrs];
            [mStr drawInRect:NSMakeRect(textX, (b.size.height - bound.size.height) / 2.0, maxW, bound.size.height + 2.0)
              withAttributes:msgAttrs];
        }

        if (self.owner->progress() > 0.001) {
            CGFloat progW = (b.size.width - 28.0) * std::clamp<CGFloat>(self.owner->progress(), 0.0, 1.0);
            NSRect progRect = NSMakeRect(14.0, b.size.height - 3.0, progW, 2.0);
            [[nsColorFromQColor(self.owner->iconColor()) colorWithAlphaComponent:0.6] setFill];
            NSRectFill(progRect);
        }
        return;
    }

    // 6. Default "center" layout (IconButton, BackToTop, ScButton, NativePlayer centerPlay)
    const CGFloat radius = self.owner->circular() ? (std::min(b.size.width, b.size.height) / 2.0)
        : (self.owner->cornerRadius() > 0 ? self.owner->cornerRadius() : (b.size.height / 2.0));

    if (!self.owner->glass()) {
        if (self.owner->fillColor().alpha() > 0) {
            NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.5, 0.5) xRadius:radius yRadius:radius];
            [nsColorFromQColor(self.owner->fillColor(), self.pressed ? 0.88 : (self.hovered ? 1.08 : 1.0)) setFill];
            [pill fill];
            if (self.owner->borderColor().alpha() > 0) {
                [nsColorFromQColor(self.owner->borderColor()) setStroke];
                pill.lineWidth = 1.0;
                [pill stroke];
            }
        } else if (self.owner->active() || self.hovered) {
            NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:b xRadius:radius yRadius:radius];
            [[nsColorFromQColor(self.owner->iconColor()) colorWithAlphaComponent:(self.owner->active() ? 0.14 : 0.08)] setFill];
            [pill fill];
        }
    } else if (self.hovered || self.pressed) {
        NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:b xRadius:radius yRadius:radius];
        [[[NSColor whiteColor] colorWithAlphaComponent:(self.pressed ? 0.12 : 0.06)] setFill];
        [pill fill];
    }

    const bool hasIcon = !self.owner->iconName().isEmpty();
    const bool hasText = !self.owner->text().isEmpty();
    if (!hasIcon && !hasText)
        return;

    NSFont *font = [NSFont systemFontOfSize:std::max(10, self.owner->fontSize())
                                     weight:(self.owner->bold() ? NSFontWeightSemibold : NSFontWeightMedium)];
    NSDictionary *attrs = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: nsColorFromQColor(hasText ? self.owner->textColor() : self.owner->iconColor())
    };
    NSString *nsText = self.owner->text().toNSString();
    NSSize textSize = hasText ? [nsText sizeWithAttributes:attrs] : NSZeroSize;

    CGFloat scale = self.pressed ? 0.88 : (self.hovered && !hasText ? 1.08 : 1.0);
    CGFloat iconDim = hasIcon ? (self.owner->iconSize() + 2.0) : 0.0;
    CGFloat spacing = (hasIcon && hasText) ? 7.0 : 0.0;
    CGFloat totalW = iconDim + spacing + textSize.width;
    const BOOL right = hasText && self.owner->textAlign() == QLatin1String("right");
    CGFloat startX = right ? b.size.width - totalW - self.owner->textRightPadding()
                           : (b.size.width - totalW) / 2.0;
    startX = std::max<CGFloat>(2.0, startX);

    NSString *sym = hasIcon ? sfSymbolForEnoName(self.owner->iconName(), self.owner->active()) : @"";

    if (hasIcon && !self.owner->iconAfter()) {
        NSRect iconRect = NSMakeRect(startX, (b.size.height - iconDim) / 2.0, iconDim, iconDim);
        drawSfSymbolInRect(sym, self.owner->iconSize() * 0.88 * scale,
                           (self.owner->bold() || self.owner->active()) ? NSFontWeightSemibold : NSFontWeightMedium,
                           nsColorFromQColor(self.owner->iconColor()), iconRect);
        startX += iconDim + spacing;
    }

    if (hasText) {
        NSRect textRect = NSMakeRect(startX, (b.size.height - textSize.height) / 2.0, textSize.width + 2.0, textSize.height);
        [nsText drawInRect:textRect withAttributes:attrs];
        startX += textSize.width + spacing;
    }

    if (hasIcon && self.owner->iconAfter()) {
        NSRect iconRect = NSMakeRect(startX, (b.size.height - iconDim) / 2.0, iconDim, iconDim);
        drawSfSymbolInRect(sym, self.owner->iconSize() * 0.88 * scale,
                           (self.owner->bold() || self.owner->active()) ? NSFontWeightSemibold : NSFontWeightMedium,
                           nsColorFromQColor(self.owner->iconColor()), iconRect);
    }
}
@end

@interface ButtonGlassView : NSGlassEffectView
@property (nonatomic, assign) NSInteger customTag;
@property (nonatomic, strong) ButtonForegroundView *foreground;
@end

@implementation ButtonGlassView
- (NSInteger)tag { return self.customTag; }
- (void)layout {
    [super layout];
    if (self.foreground) {
        self.foreground.frame = self.bounds;
    }
}
- (void)scrollWheel:(NSEvent *)event {
    [self.superview scrollWheel:event];
}
@end

#endif // Q_OS_MACOS

namespace sc {

QImage renderSfSymbol(const QString &name, const QColor &color, const QSize &target, bool active) {
#ifdef Q_OS_MACOS
    if (name.isEmpty() || target.width() <= 0 || target.height() <= 0)
        return {};
    @autoreleasepool {
        NSString *sym = sfSymbolForEnoName(name, active);
        NSImage *base = [NSImage imageWithSystemSymbolName:sym accessibilityDescription:nil];
        if (!base)
            return {};
        const CGFloat ptSize = std::min(target.width(), target.height()) * 0.72;
        NSImageSymbolConfiguration *cfg = [NSImageSymbolConfiguration configurationWithPointSize:ptSize
                                                                                          weight:(active ? NSFontWeightSemibold : NSFontWeightMedium)];
        NSImage *configured = [base imageWithSymbolConfiguration:cfg];
        if (!configured)
            configured = base;

        NSSize imgSize = configured.size;
        if (imgSize.width <= 0 || imgSize.height <= 0)
            imgSize = NSMakeSize(target.width(), target.height());

        const CGFloat pad = std::max<CGFloat>(1.0, std::min(target.width(), target.height()) * 0.08);
        const CGFloat availW = std::max<CGFloat>(1.0, target.width() - 2.0 * pad);
        const CGFloat availH = std::max<CGFloat>(1.0, target.height() - 2.0 * pad);
        const CGFloat scale = std::min(availW / imgSize.width, availH / imgSize.height);
        const CGFloat drawW = imgSize.width * scale;
        const CGFloat drawH = imgSize.height * scale;

        NSRect propRect = NSMakeRect(0, 0, drawW, drawH);
        CGImageRef cgImg = [configured CGImageForProposedRect:&propRect context:nil hints:nil];
        if (!cgImg)
            return {};

        QImage out(target, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            out.bits(), out.width(), out.height(), 8, out.bytesPerLine(), cs,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
        if (ctx) {
            CGRect drawRect = CGRectMake((target.width() - drawW) / 2.0, (target.height() - drawH) / 2.0, drawW, drawH);
            CGContextDrawImage(ctx, drawRect, cgImg);
            CGContextRelease(ctx);
        }
        CGColorSpaceRelease(cs);

        QPainter painter(&out);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(out.rect(), color.isValid() ? color : QColor(Qt::white));
        painter.end();
        return out;
    }
#else
    Q_UNUSED(name)
    Q_UNUSED(color)
    Q_UNUSED(target)
    Q_UNUSED(active)
    return {};
#endif
}

bool NativeGlassPlate::isSupported() {
#ifdef Q_OS_MACOS
    if (@available(macOS 26.0, *)) {
        return NSClassFromString(@"NSGlassEffectView") != nil;
    }
#endif
    return false;
}

bool NativeGlassButton::isSupported() {
#ifdef Q_OS_MACOS
    if (@available(macOS 26.0, *)) {
        return NSClassFromString(@"NSGlassEffectView") != nil;
    }
#endif
    return false;
}

// -----------------------------------------------------------------------------
// NativeGlassPlate
// -----------------------------------------------------------------------------

NativeGlassPlate::NativeGlassPlate(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, false);
}

NativeGlassPlate::~NativeGlassPlate() {
    destroyNativeView();
}

void NativeGlassPlate::setCornerRadius(qreal r) {
    if (qFuzzyCompare(m_cornerRadius, r))
        return;
    m_cornerRadius = r;
    emit cornerRadiusChanged();
    syncNativeView();
}

void NativeGlassPlate::setStyle(const QString &s) {
    if (m_style == s)
        return;
    m_style = s;
    emit styleChanged();
    syncNativeView();
}

void NativeGlassPlate::setInteractive(bool i) {
    if (m_interactive == i)
        return;
    m_interactive = i;
    emit interactiveChanged();
    syncNativeView();
}

void NativeGlassPlate::setTintColor(const QColor &c) {
    if (m_tintColor == c)
        return;
    m_tintColor = c;
    emit tintColorChanged();
    syncNativeView();
}

void NativeGlassPlate::setPassthrough(bool p) {
    if (m_passthrough == p)
        return;
    m_passthrough = p;
    emit passthroughChanged();
    syncNativeView();
}

void NativeGlassPlate::setIsContainer(bool c) {
    if (m_isContainer == c)
        return;
    m_isContainer = c;
    emit isContainerChanged();
    destroyNativeView();
    syncNativeView();
}

void NativeGlassPlate::setZTier(int t) {
    if (m_zTier == t)
        return;
    m_zTier = t;
    emit zTierChanged();
    destroyNativeView();
    syncNativeView();
}

void NativeGlassPlate::setIconName(const QString &i) {
    if (m_iconName == i)
        return;
    m_iconName = i;
    emit iconNameChanged();
    syncNativeView();
}

void NativeGlassPlate::setIconSize(int s) {
    if (m_iconSize == s)
        return;
    m_iconSize = s;
    emit iconSizeChanged();
    syncNativeView();
}

void NativeGlassPlate::setIconColor(const QColor &c) {
    if (m_iconColor == c)
        return;
    m_iconColor = c;
    emit iconColorChanged();
    syncNativeView();
}

void NativeGlassPlate::setText(const QString &t) {
    if (m_text == t)
        return;
    m_text = t;
    emit textChanged();
    syncNativeView();
}

void NativeGlassPlate::setTextColor(const QColor &c) {
    if (m_textColor == c)
        return;
    m_textColor = c;
    emit textColorChanged();
    syncNativeView();
}

void NativeGlassPlate::setFontSize(int s) {
    if (m_fontSize == s)
        return;
    m_fontSize = s;
    emit fontSizeChanged();
    syncNativeView();
}

void NativeGlassPlate::setBold(bool b) {
    if (m_bold == b)
        return;
    m_bold = b;
    emit boldChanged();
    syncNativeView();
}

void NativeGlassPlate::setTextAlign(const QString &a) {
    if (m_textAlign == a)
        return;
    m_textAlign = a;
    emit textAlignChanged();
    syncNativeView();
}

void NativeGlassPlate::setTextRightPadding(int p) {
    if (m_textRightPadding == p)
        return;
    m_textRightPadding = p;
    emit textRightPaddingChanged();
    syncNativeView();
}

void NativeGlassPlate::itemChange(ItemChange change, const ItemChangeData &data) {
    QQuickItem::itemChange(change, data);
    if (change == ItemSceneChange) {
        if (data.window) {
            connect(data.window, &QQuickWindow::afterAnimating, this, &NativeGlassPlate::updateNativeGeometry, Qt::UniqueConnection);
            connect(data.window, &QQuickWindow::widthChanged, this, &NativeGlassPlate::updateNativeGeometry, Qt::UniqueConnection);
            connect(data.window, &QQuickWindow::heightChanged, this, &NativeGlassPlate::updateNativeGeometry, Qt::UniqueConnection);
            syncNativeView();
        } else {
            destroyNativeView();
        }
    } else if (change == ItemVisibleHasChanged || change == ItemOpacityHasChanged) {
        updateNativeGeometry();
    }
}

void NativeGlassPlate::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateNativeGeometry();
}

void NativeGlassPlate::syncNativeView() {
#ifdef Q_OS_MACOS
    if (!window() || !isSupported())
        return;

    @autoreleasepool {
        NSView *qnsView = reinterpret_cast<NSView *>(window()->winId());
        if (!qnsView)
            return;

        const NSInteger targetTag = (m_zTier > 0) ? 7003 : 7001;

        if (!m_nativeView) {
            if (@available(macOS 26.0, *)) {
                if (m_isContainer) {
                    Class containerCls = NSClassFromString(@"NSGlassEffectContainerView");
                    if (containerCls) {
                        NSGlassEffectContainerView *container = [[containerCls alloc] initWithFrame:NSZeroRect];
                        m_nativeView = container;
                        insertNativeSubview(qnsView, container, targetTag);
                    }
                } else {
                    PassthroughGlassView *glass = [[PassthroughGlassView alloc] initWithFrame:NSZeroRect];
                    glass.customTag = targetTag;
                    PlateContentView *content = [[PlateContentView alloc] initWithFrame:NSZeroRect];
                    glass.plateContent = content;
                    glass.contentView = content;
                    m_nativeView = glass;
                    insertNativeSubview(qnsView, glass, targetTag);
                }
            }
        }

        if (!m_nativeView)
            return;

        if (@available(macOS 26.0, *)) {
            if (!m_isContainer) {
                PassthroughGlassView *glass = static_cast<PassthroughGlassView *>(m_nativeView);
                glass.passthrough = m_passthrough;
                glass.cornerRadius = m_cornerRadius;
                glass.style = (m_style == QLatin1String("clear")) ? NSGlassEffectViewStyleClear : NSGlassEffectViewStyleRegular;
                if (@available(macOS 27.0, *)) {
                    if ([glass respondsToSelector:@selector(setEffectIsInteractive:)]) {
                        glass.effectIsInteractive = m_interactive;
                    }
                }
                // Follow macOS 27 cmux rule: clamp tint alpha so glass never turns into an opaque block
                if (m_tintColor.isValid() && m_tintColor.alpha() > 10) {
                    const qreal safeAlpha = std::min<qreal>(m_tintColor.alphaF(), 0.36);
                    glass.tintColor = [NSColor colorWithRed:m_tintColor.redF() green:m_tintColor.greenF() blue:m_tintColor.blueF() alpha:safeAlpha];
                } else {
                    glass.tintColor = nil;
                }

                if (glass.plateContent) {
                    glass.plateContent.iconName = m_iconName.isEmpty() ? @"" : sfSymbolForEnoName(m_iconName, false);
                    glass.plateContent.iconSize = m_iconSize;
                    glass.plateContent.iconColor = nsColorFromQColor(m_iconColor);
                    glass.plateContent.text = m_text.toNSString();
                    glass.plateContent.textColor = nsColorFromQColor(m_textColor);
                    glass.plateContent.fontSize = m_fontSize;
                    glass.plateContent.bold = m_bold;
                    glass.plateContent.textAlign = m_textAlign.toNSString();
                    glass.plateContent.textRightPadding = m_textRightPadding;
                    [glass.plateContent setNeedsDisplay:YES];
                }
            }
        }
        updateNativeGeometry();
    }
#endif
}

void NativeGlassPlate::updateNativeGeometry() {
#ifdef Q_OS_MACOS
    if (!window() || !m_nativeView)
        return;

    @autoreleasepool {
        NSView *qnsView = reinterpret_cast<NSView *>(window()->winId());
        if (!qnsView)
            return;

        NSView *nativeView = static_cast<NSView *>(m_nativeView);
        QRectF sceneRect;
        qreal effOpacity = 1.0;
        if (!computeEffectiveVisibility(this, &sceneRect, &effOpacity)) {
            nativeView.hidden = YES;
            return;
        }

        nativeView.hidden = NO;
        nativeView.alphaValue = effOpacity;

        CGFloat x = sceneRect.x();
        CGFloat y = sceneRect.y();
        CGFloat w = sceneRect.width();
        CGFloat h = sceneRect.height();

        if (!qnsView.isFlipped) {
            y = qnsView.bounds.size.height - y - h;
        }

        nativeView.frame = NSMakeRect(x, y, w, h);
        if (!m_isContainer) {
            PassthroughGlassView *glass = static_cast<PassthroughGlassView *>(m_nativeView);
            if (glass.plateContent) {
                glass.plateContent.frame = glass.bounds;
            }
        }
    }
#endif
}

void NativeGlassPlate::destroyNativeView() {
#ifdef Q_OS_MACOS
    if (m_nativeView) {
        @autoreleasepool {
            NSView *v = static_cast<NSView *>(m_nativeView);
            [v removeFromSuperview];
        }
        m_nativeView = nullptr;
    }
#endif
}

// -----------------------------------------------------------------------------
// NativeGlassButton
// -----------------------------------------------------------------------------

NativeGlassButton::NativeGlassButton(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, false);
}

NativeGlassButton::~NativeGlassButton() {
    destroyNativeView();
}

void NativeGlassButton::setGlass(bool g) {
    if (m_glass == g) return;
    m_glass = g;
    emit glassChanged();
    destroyNativeView();
    syncNativeView();
}

void NativeGlassButton::setPassthrough(bool p) {
    if (m_passthrough == p) return;
    m_passthrough = p;
    emit passthroughChanged();
    syncNativeView();
}

void NativeGlassButton::setZTier(int t) {
    if (m_zTier == t) return;
    m_zTier = t;
    emit zTierChanged();
    destroyNativeView();
    syncNativeView();
}

void NativeGlassButton::setLayout(const QString &l) {
    if (m_layout == l) return;
    m_layout = l;
    emit layoutChanged();
    syncNativeView();
}

void NativeGlassButton::setText(const QString &t) {
    if (m_text == t) return;
    m_text = t;
    emit textChanged();
    syncNativeView();
}

void NativeGlassButton::setTextColor(const QColor &c) {
    if (m_textColor == c) return;
    m_textColor = c;
    emit textColorChanged();
    syncNativeView();
}

void NativeGlassButton::setFontSize(int s) {
    if (m_fontSize == s) return;
    m_fontSize = s;
    emit fontSizeChanged();
    syncNativeView();
}

void NativeGlassButton::setBold(bool b) {
    if (m_bold == b) return;
    m_bold = b;
    emit boldChanged();
    syncNativeView();
}

void NativeGlassButton::setTextAlign(const QString &a) {
    if (m_textAlign == a) return;
    m_textAlign = a;
    emit textAlignChanged();
    syncNativeView();
}

void NativeGlassButton::setTextRightPadding(int p) {
    if (m_textRightPadding == p) return;
    m_textRightPadding = p;
    emit textRightPaddingChanged();
    syncNativeView();
}

void NativeGlassButton::setIconName(const QString &i) {
    if (m_iconName == i) return;
    m_iconName = i;
    emit iconNameChanged();
    syncNativeView();
}

void NativeGlassButton::setIconSize(int s) {
    if (m_iconSize == s) return;
    m_iconSize = s;
    emit iconSizeChanged();
    syncNativeView();
}

void NativeGlassButton::setIconColor(const QColor &c) {
    if (m_iconColor == c) return;
    m_iconColor = c;
    emit iconColorChanged();
    syncNativeView();
}

void NativeGlassButton::setIconAfter(bool a) {
    if (m_iconAfter == a) return;
    m_iconAfter = a;
    emit iconAfterChanged();
    syncNativeView();
}

void NativeGlassButton::setImageSource(const QString &s) {
    if (m_imageSource == s) return;
    m_imageSource = s;
    emit imageSourceChanged();
    syncNativeView();
}

void NativeGlassButton::setCircular(bool c) {
    if (m_circular == c) return;
    m_circular = c;
    emit circularChanged();
    syncNativeView();
}

void NativeGlassButton::setCornerRadius(qreal r) {
    if (qFuzzyCompare(m_cornerRadius, r)) return;
    m_cornerRadius = r;
    emit cornerRadiusChanged();
    syncNativeView();
}

void NativeGlassButton::setStyle(const QString &s) {
    if (m_style == s) return;
    m_style = s;
    emit styleChanged();
    syncNativeView();
}

void NativeGlassButton::setInteractive(bool i) {
    if (m_interactive == i) return;
    m_interactive = i;
    emit interactiveChanged();
    syncNativeView();
}

void NativeGlassButton::setActive(bool a) {
    if (m_active == a) return;
    m_active = a;
    emit activeChanged();
    syncNativeView();
}

void NativeGlassButton::setCollapsed(bool c) {
    if (m_collapsed == c) return;
    m_collapsed = c;
    emit collapsedChanged();
    syncNativeView();
}

void NativeGlassButton::setFloating(bool f) {
    if (m_floating == f) return;
    m_floating = f;
    emit floatingChanged();
    syncNativeView();
}

void NativeGlassButton::setIsSearch(bool s) {
    if (m_isSearch == s) return;
    m_isSearch = s;
    emit isSearchChanged();
    syncNativeView();
}

void NativeGlassButton::setIndicatorSide(const QString &s) {
    if (m_indicatorSide == s) return;
    m_indicatorSide = s;
    emit indicatorSideChanged();
    syncNativeView();
}

void NativeGlassButton::setTintColor(const QColor &c) {
    if (m_tintColor == c) return;
    m_tintColor = c;
    emit tintColorChanged();
    syncNativeView();
}

void NativeGlassButton::setFillColor(const QColor &c) {
    if (m_fillColor == c) return;
    m_fillColor = c;
    emit fillColorChanged();
    syncNativeView();
}

void NativeGlassButton::setBorderColor(const QColor &c) {
    if (m_borderColor == c) return;
    m_borderColor = c;
    emit borderColorChanged();
    syncNativeView();
}

void NativeGlassButton::setAccentColor(const QColor &c) {
    if (m_accentColor == c) return;
    m_accentColor = c;
    emit accentColorChanged();
    syncNativeView();
}

void NativeGlassButton::setBadgeCount(int c) {
    if (m_badgeCount == c) return;
    m_badgeCount = c;
    emit badgeCountChanged();
    syncNativeView();
}

void NativeGlassButton::setBadgeText(const QString &t) {
    if (m_badgeText == t) return;
    m_badgeText = t;
    emit badgeTextChanged();
    syncNativeView();
}

void NativeGlassButton::setBadgeTextColor(const QColor &c) {
    if (m_badgeTextColor == c) return;
    m_badgeTextColor = c;
    emit badgeTextColorChanged();
    syncNativeView();
}

void NativeGlassButton::setRightInset(qreal r) {
    if (qFuzzyCompare(m_rightInset, r)) return;
    m_rightInset = r;
    emit rightInsetChanged();
    syncNativeView();
}

void NativeGlassButton::setProgress(qreal p) {
    if (qFuzzyCompare(m_progress, p)) return;
    m_progress = p;
    emit progressChanged();
    syncNativeView();
}

void NativeGlassButton::setTip(const QString &t) {
    if (m_tip == t) return;
    m_tip = t;
    emit tipChanged();
    syncNativeView();
}

void NativeGlassButton::updateHovered(bool h) {
    if (m_hovered == h) return;
    m_hovered = h;
    emit hoveredChanged();
}

void NativeGlassButton::updatePressed(bool p) {
    if (m_pressed == p) return;
    m_pressed = p;
    emit pressedChanged();
}

void NativeGlassButton::emitClicked() {
    emit clicked();
}

void NativeGlassButton::itemChange(ItemChange change, const ItemChangeData &data) {
    QQuickItem::itemChange(change, data);
    if (change == ItemSceneChange) {
        if (data.window) {
            connect(data.window, &QQuickWindow::afterAnimating, this, &NativeGlassButton::updateNativeGeometry, Qt::UniqueConnection);
            connect(data.window, &QQuickWindow::widthChanged, this, &NativeGlassButton::updateNativeGeometry, Qt::UniqueConnection);
            connect(data.window, &QQuickWindow::heightChanged, this, &NativeGlassButton::updateNativeGeometry, Qt::UniqueConnection);
            syncNativeView();
        } else {
            destroyNativeView();
        }
    } else if (change == ItemVisibleHasChanged || change == ItemOpacityHasChanged || change == ItemEnabledHasChanged) {
        updateNativeGeometry();
    }
}

void NativeGlassButton::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateNativeGeometry();
}

void NativeGlassButton::syncNativeView() {
#ifdef Q_OS_MACOS
    if (!window() || !isSupported())
        return;

    @autoreleasepool {
        NSView *qnsView = reinterpret_cast<NSView *>(window()->winId());
        if (!qnsView)
            return;

        const NSInteger targetTag = (m_zTier > 0) ? 7004 : 7002;

        if (!m_nativeButton) {
            if (@available(macOS 26.0, *)) {
                if (m_glass) {
                    ButtonGlassView *glass = [[ButtonGlassView alloc] initWithFrame:NSZeroRect];
                    glass.customTag = targetTag;
                    ButtonForegroundView *fg = [[ButtonForegroundView alloc] initWithFrame:NSZeroRect];
                    fg.owner = this;
                    fg.customTag = targetTag;
                    glass.foreground = fg;
                    glass.contentView = fg;
                    m_nativeButton = glass;
                    insertNativeSubview(qnsView, glass, targetTag);
                } else {
                    ButtonForegroundView *fg = [[ButtonForegroundView alloc] initWithFrame:NSZeroRect];
                    fg.owner = this;
                    fg.customTag = targetTag;
                    m_nativeButton = fg;
                    insertNativeSubview(qnsView, fg, targetTag);
                }
            }
        }

        if (!m_nativeButton)
            return;

        if (@available(macOS 26.0, *)) {
            ButtonForegroundView *fg = nil;
            if (m_glass) {
                ButtonGlassView *glass = static_cast<ButtonGlassView *>(m_nativeButton);
                fg = glass.foreground;
                const CGFloat radius = m_circular ? (std::min(width(), height()) / 2.0)
                    : (m_cornerRadius > 0 ? m_cornerRadius : (height() / 2.0));
                glass.cornerRadius = radius;
                glass.style = (m_style == QLatin1String("clear")) ? NSGlassEffectViewStyleClear : NSGlassEffectViewStyleRegular;
                if (@available(macOS 27.0, *)) {
                    if ([glass respondsToSelector:@selector(setEffectIsInteractive:)]) {
                        glass.effectIsInteractive = m_interactive;
                    }
                }
                QColor effectiveTint = m_tintColor.alpha() > 0 ? m_tintColor : m_fillColor;
                if (effectiveTint.isValid() && effectiveTint.alpha() > 10) {
                    const qreal safeAlpha = std::min<qreal>(effectiveTint.alphaF(), 0.42);
                    glass.tintColor = [NSColor colorWithRed:effectiveTint.redF()
                                                      green:effectiveTint.greenF()
                                                       blue:effectiveTint.blueF()
                                                      alpha:safeAlpha];
                } else {
                    glass.tintColor = nil;
                }
            } else {
                fg = static_cast<ButtonForegroundView *>(m_nativeButton);
            }

            if (fg) {
                fg.toolTip = m_tip.isEmpty() ? nil : m_tip.toNSString();
                [fg.window invalidateCursorRectsForView:fg];
                [fg setNeedsDisplay:YES];
            }
        }

        updateNativeGeometry();
    }
#endif
}

void NativeGlassButton::updateNativeGeometry() {
#ifdef Q_OS_MACOS
    if (!window() || !m_nativeButton)
        return;

    @autoreleasepool {
        NSView *qnsView = reinterpret_cast<NSView *>(window()->winId());
        if (!qnsView)
            return;

        NSView *nativeView = static_cast<NSView *>(m_nativeButton);
        QRectF sceneRect;
        qreal effOpacity = 1.0;
        if (!computeEffectiveVisibility(this, &sceneRect, &effOpacity)) {
            if (!nativeView.hidden) {
                nativeView.hidden = YES;
                updateHovered(false);
                updatePressed(false);
            }
            return;
        }

        nativeView.hidden = NO;
        nativeView.alphaValue = effOpacity * (isEnabled() ? 1.0 : 0.4);

        CGFloat x = sceneRect.x();
        CGFloat y = sceneRect.y();
        CGFloat w = sceneRect.width();
        CGFloat h = sceneRect.height();

        if (!qnsView.isFlipped) {
            y = qnsView.bounds.size.height - y - h;
        }

        nativeView.frame = NSMakeRect(x, y, w, h);
        if (m_glass) {
            ButtonGlassView *glass = static_cast<ButtonGlassView *>(m_nativeButton);
            glass.cornerRadius = m_circular ? (std::min(w, h) / 2.0) : (m_cornerRadius > 0 ? m_cornerRadius : (h / 2.0));
            if (glass.foreground) {
                glass.foreground.frame = glass.bounds;
                [glass.foreground setNeedsDisplay:YES];
            }
        } else {
            [nativeView setNeedsDisplay:YES];
        }
    }
#endif
}

void NativeGlassButton::destroyNativeView() {
#ifdef Q_OS_MACOS
    if (m_nativeButton) {
        @autoreleasepool {
            NSView *v = static_cast<NSView *>(m_nativeButton);
            if ([v isKindOfClass:[ButtonGlassView class]]) {
                ButtonGlassView *g = static_cast<ButtonGlassView *>(v);
                if (g.foreground)
                    g.foreground.owner = nullptr;
            } else if ([v isKindOfClass:[ButtonForegroundView class]]) {
                ButtonForegroundView *fg = static_cast<ButtonForegroundView *>(v);
                fg.owner = nullptr;
            }
            [v removeFromSuperview];
        }
        m_nativeButton = nullptr;
    }
#endif
}

} // namespace sc
