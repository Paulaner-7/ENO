#include "NativeGlass.h"

#ifndef Q_OS_MACOS

namespace sc {

QImage renderSfSymbol(const QString &, const QColor &, const QSize &, bool) {
    return {};
}

bool NativeGlassPlate::isSupported() {
    return false;
}

bool NativeGlassButton::isSupported() {
    return false;
}

NativeGlassPlate::NativeGlassPlate(QQuickItem *parent) : QQuickItem(parent) {}
NativeGlassPlate::~NativeGlassPlate() = default;
void NativeGlassPlate::setCornerRadius(qreal) {}
void NativeGlassPlate::setStyle(const QString &) {}
void NativeGlassPlate::setInteractive(bool) {}
void NativeGlassPlate::setTintColor(const QColor &) {}
void NativeGlassPlate::setPassthrough(bool) {}
void NativeGlassPlate::setIsContainer(bool) {}
void NativeGlassPlate::setZTier(int) {}
void NativeGlassPlate::setIconName(const QString &) {}
void NativeGlassPlate::setIconSize(int) {}
void NativeGlassPlate::setIconColor(const QColor &) {}
void NativeGlassPlate::setText(const QString &) {}
void NativeGlassPlate::setTextColor(const QColor &) {}
void NativeGlassPlate::setFontSize(int) {}
void NativeGlassPlate::setBold(bool) {}
void NativeGlassPlate::itemChange(ItemChange change, const ItemChangeData &data) { QQuickItem::itemChange(change, data); }
void NativeGlassPlate::geometryChange(const QRectF &n, const QRectF &o) { QQuickItem::geometryChange(n, o); }
void NativeGlassPlate::updateNativeGeometry() {}
void NativeGlassPlate::syncNativeView() {}
void NativeGlassPlate::destroyNativeView() {}

NativeGlassButton::NativeGlassButton(QQuickItem *parent) : QQuickItem(parent) {}
NativeGlassButton::~NativeGlassButton() = default;
void NativeGlassButton::setGlass(bool) {}
void NativeGlassButton::setPassthrough(bool) {}
void NativeGlassButton::setZTier(int) {}
void NativeGlassButton::setLayout(const QString &) {}
void NativeGlassButton::setText(const QString &) {}
void NativeGlassButton::setTextColor(const QColor &) {}
void NativeGlassButton::setFontSize(int) {}
void NativeGlassButton::setBold(bool) {}
void NativeGlassButton::setIconName(const QString &) {}
void NativeGlassButton::setIconSize(int) {}
void NativeGlassButton::setIconColor(const QColor &) {}
void NativeGlassButton::setIconAfter(bool) {}
void NativeGlassButton::setImageSource(const QString &) {}
void NativeGlassButton::setCircular(bool) {}
void NativeGlassButton::setCornerRadius(qreal) {}
void NativeGlassButton::setStyle(const QString &) {}
void NativeGlassButton::setInteractive(bool) {}
void NativeGlassButton::setActive(bool) {}
void NativeGlassButton::setCollapsed(bool) {}
void NativeGlassButton::setFloating(bool) {}
void NativeGlassButton::setIsSearch(bool) {}
void NativeGlassButton::setIndicatorSide(const QString &) {}
void NativeGlassButton::setTintColor(const QColor &) {}
void NativeGlassButton::setFillColor(const QColor &) {}
void NativeGlassButton::setBorderColor(const QColor &) {}
void NativeGlassButton::setAccentColor(const QColor &) {}
void NativeGlassButton::setBadgeCount(int) {}
void NativeGlassButton::setBadgeText(const QString &) {}
void NativeGlassButton::setBadgeTextColor(const QColor &) {}
void NativeGlassButton::setRightInset(qreal) {}
void NativeGlassButton::setProgress(qreal) {}
void NativeGlassButton::setTip(const QString &) {}
void NativeGlassButton::updateHovered(bool) {}
void NativeGlassButton::updatePressed(bool) {}
void NativeGlassButton::emitClicked() { emit clicked(); }
void NativeGlassButton::itemChange(ItemChange change, const ItemChangeData &data) { QQuickItem::itemChange(change, data); }
void NativeGlassButton::geometryChange(const QRectF &n, const QRectF &o) { QQuickItem::geometryChange(n, o); }
void NativeGlassButton::updateNativeGeometry() {}
void NativeGlassButton::syncNativeView() {}
void NativeGlassButton::destroyNativeView() {}

} // namespace sc

#endif
