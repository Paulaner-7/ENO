#pragma once

#include <QColor>
#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSize>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace sc {

// Renders a macOS Apple SF Symbol (mapped from ENO/Lucide icon names) at the
// requested pixel dimensions and tinted to `color`. Returns a null QImage if
// unavailable so callers can fall back to SVG.
QImage renderSfSymbol(const QString &name, const QColor &color, const QSize &target, bool active = false);

class NativeGlassPlate : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(bool interactive READ interactive WRITE setInteractive NOTIFY interactiveChanged)
    Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor NOTIFY tintColorChanged)
    Q_PROPERTY(bool passthrough READ passthrough WRITE setPassthrough NOTIFY passthroughChanged)
    Q_PROPERTY(bool isContainer READ isContainer WRITE setIsContainer NOTIFY isContainerChanged)
    Q_PROPERTY(int zTier READ zTier WRITE setZTier NOTIFY zTierChanged)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY iconColorChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY boldChanged)
    // "center" (default) or "right": where the text sits in the plate. The
    // player uses right for the time beside its native control icons.
    Q_PROPERTY(QString textAlign READ textAlign WRITE setTextAlign NOTIFY textAlignChanged)
    Q_PROPERTY(int textRightPadding READ textRightPadding WRITE setTextRightPadding NOTIFY textRightPaddingChanged)
    Q_PROPERTY(bool isSupported READ isSupported CONSTANT)

public:
    explicit NativeGlassPlate(QQuickItem *parent = nullptr);
    ~NativeGlassPlate() override;

    qreal cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(qreal r);

    QString style() const { return m_style; }
    void setStyle(const QString &s);

    bool interactive() const { return m_interactive; }
    void setInteractive(bool i);

    QColor tintColor() const { return m_tintColor; }
    void setTintColor(const QColor &c);

    bool passthrough() const { return m_passthrough; }
    void setPassthrough(bool p);

    bool isContainer() const { return m_isContainer; }
    void setIsContainer(bool c);

    int zTier() const { return m_zTier; }
    void setZTier(int t);

    QString iconName() const { return m_iconName; }
    void setIconName(const QString &i);

    int iconSize() const { return m_iconSize; }
    void setIconSize(int s);

    QColor iconColor() const { return m_iconColor; }
    void setIconColor(const QColor &c);

    QString text() const { return m_text; }
    void setText(const QString &t);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &c);

    int fontSize() const { return m_fontSize; }
    void setFontSize(int s);

    bool bold() const { return m_bold; }
    void setBold(bool b);

    QString textAlign() const { return m_textAlign; }
    void setTextAlign(const QString &a);

    int textRightPadding() const { return m_textRightPadding; }
    void setTextRightPadding(int p);

    static bool isSupported();

signals:
    void cornerRadiusChanged();
    void styleChanged();
    void interactiveChanged();
    void tintColorChanged();
    void passthroughChanged();
    void isContainerChanged();
    void zTierChanged();
    void iconNameChanged();
    void iconSizeChanged();
    void iconColorChanged();
    void textChanged();
    void textColorChanged();
    void fontSizeChanged();
    void boldChanged();
    void textAlignChanged();
    void textRightPaddingChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void updateNativeGeometry();

private:
    void syncNativeView();
    void destroyNativeView();

    qreal m_cornerRadius = 16.0;
    QString m_style = QStringLiteral("regular");
    bool m_interactive = true;
    QColor m_tintColor = Qt::transparent;
    bool m_passthrough = true;
    bool m_isContainer = false;
    int m_zTier = 0;

    QString m_iconName;
    int m_iconSize = 14;
    QColor m_iconColor = Qt::white;
    QString m_text;
    QColor m_textColor = Qt::white;
    int m_fontSize = 12;
    bool m_bold = false;
    QString m_textAlign = QStringLiteral("center");
    int m_textRightPadding = 8;

    void *m_nativeView = nullptr;
};

class NativeGlassButton : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool glass READ glass WRITE setGlass NOTIFY glassChanged)
    Q_PROPERTY(bool passthrough READ passthrough WRITE setPassthrough NOTIFY passthroughChanged)
    Q_PROPERTY(int zTier READ zTier WRITE setZTier NOTIFY zTierChanged)
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY boldChanged)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY iconColorChanged)
    Q_PROPERTY(bool iconAfter READ iconAfter WRITE setIconAfter NOTIFY iconAfterChanged)
    Q_PROPERTY(QString imageSource READ imageSource WRITE setImageSource NOTIFY imageSourceChanged)
    Q_PROPERTY(bool circular READ circular WRITE setCircular NOTIFY circularChanged)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(bool interactive READ interactive WRITE setInteractive NOTIFY interactiveChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool collapsed READ collapsed WRITE setCollapsed NOTIFY collapsedChanged)
    Q_PROPERTY(bool floating READ floating WRITE setFloating NOTIFY floatingChanged)
    Q_PROPERTY(bool isSearch READ isSearch WRITE setIsSearch NOTIFY isSearchChanged)
    Q_PROPERTY(QString indicatorSide READ indicatorSide WRITE setIndicatorSide NOTIFY indicatorSideChanged)
    Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor NOTIFY tintColorChanged)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(int badgeCount READ badgeCount WRITE setBadgeCount NOTIFY badgeCountChanged)
    Q_PROPERTY(QString badgeText READ badgeText WRITE setBadgeText NOTIFY badgeTextChanged)
    Q_PROPERTY(QColor badgeTextColor READ badgeTextColor WRITE setBadgeTextColor NOTIFY badgeTextColorChanged)
    Q_PROPERTY(qreal rightInset READ rightInset WRITE setRightInset NOTIFY rightInsetChanged)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(QString tip READ tip WRITE setTip NOTIFY tipChanged)
    Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)
    Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
    Q_PROPERTY(bool isSupported READ isSupported CONSTANT)

public:
    explicit NativeGlassButton(QQuickItem *parent = nullptr);
    ~NativeGlassButton() override;

    bool glass() const { return m_glass; }
    void setGlass(bool g);

    bool passthrough() const { return m_passthrough; }
    void setPassthrough(bool p);

    int zTier() const { return m_zTier; }
    void setZTier(int t);

    QString layout() const { return m_layout; }
    void setLayout(const QString &l);

    QString text() const { return m_text; }
    void setText(const QString &t);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &c);

    int fontSize() const { return m_fontSize; }
    void setFontSize(int s);

    bool bold() const { return m_bold; }
    void setBold(bool b);

    QString iconName() const { return m_iconName; }
    void setIconName(const QString &i);

    int iconSize() const { return m_iconSize; }
    void setIconSize(int s);

    QColor iconColor() const { return m_iconColor; }
    void setIconColor(const QColor &c);

    bool iconAfter() const { return m_iconAfter; }
    void setIconAfter(bool a);

    QString imageSource() const { return m_imageSource; }
    void setImageSource(const QString &s);

    bool circular() const { return m_circular; }
    void setCircular(bool c);

    qreal cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(qreal r);

    QString style() const { return m_style; }
    void setStyle(const QString &s);

    bool interactive() const { return m_interactive; }
    void setInteractive(bool i);

    bool active() const { return m_active; }
    void setActive(bool a);

    bool collapsed() const { return m_collapsed; }
    void setCollapsed(bool c);

    bool floating() const { return m_floating; }
    void setFloating(bool f);

    bool isSearch() const { return m_isSearch; }
    void setIsSearch(bool s);

    QString indicatorSide() const { return m_indicatorSide; }
    void setIndicatorSide(const QString &s);

    QColor tintColor() const { return m_tintColor; }
    void setTintColor(const QColor &c);

    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor &c);

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor &c);

    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor &c);

    int badgeCount() const { return m_badgeCount; }
    void setBadgeCount(int c);

    QString badgeText() const { return m_badgeText; }
    void setBadgeText(const QString &t);

    QColor badgeTextColor() const { return m_badgeTextColor; }
    void setBadgeTextColor(const QColor &c);

    qreal rightInset() const { return m_rightInset; }
    void setRightInset(qreal r);

    qreal progress() const { return m_progress; }
    void setProgress(qreal p);

    QString tip() const { return m_tip; }
    void setTip(const QString &t);

    bool hovered() const { return m_hovered; }
    void updateHovered(bool h);

    bool pressed() const { return m_pressed; }
    void updatePressed(bool p);

    static bool isSupported();

    void emitClicked();

signals:
    void glassChanged();
    void passthroughChanged();
    void zTierChanged();
    void layoutChanged();
    void textChanged();
    void textColorChanged();
    void fontSizeChanged();
    void boldChanged();
    void iconNameChanged();
    void iconSizeChanged();
    void iconColorChanged();
    void iconAfterChanged();
    void imageSourceChanged();
    void circularChanged();
    void cornerRadiusChanged();
    void styleChanged();
    void interactiveChanged();
    void activeChanged();
    void collapsedChanged();
    void floatingChanged();
    void isSearchChanged();
    void indicatorSideChanged();
    void tintColorChanged();
    void fillColorChanged();
    void borderColorChanged();
    void accentColorChanged();
    void badgeCountChanged();
    void badgeTextChanged();
    void badgeTextColorChanged();
    void rightInsetChanged();
    void progressChanged();
    void tipChanged();
    void hoveredChanged();
    void pressedChanged();
    void clicked();

protected:
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void updateNativeGeometry();

private:
    void syncNativeView();
    void destroyNativeView();

    bool m_glass = true;
    bool m_passthrough = false;
    int m_zTier = 0;
    QString m_layout = QStringLiteral("center");
    QString m_text;
    QColor m_textColor = Qt::white;
    int m_fontSize = 13;
    bool m_bold = false;
    QString m_iconName;
    int m_iconSize = 16;
    QColor m_iconColor = Qt::white;
    bool m_iconAfter = false;
    QString m_imageSource;
    bool m_circular = false;
    qreal m_cornerRadius = 0.0;
    QString m_style = QStringLiteral("clear");
    bool m_interactive = true;
    bool m_active = false;
    bool m_collapsed = false;
    bool m_floating = false;
    bool m_isSearch = false;
    QString m_indicatorSide = QStringLiteral("left");
    QColor m_tintColor = Qt::transparent;
    QColor m_fillColor = Qt::transparent;
    QColor m_borderColor = Qt::transparent;
    QColor m_accentColor = QColor(99, 102, 241);
    int m_badgeCount = 0;
    QString m_badgeText;
    QColor m_badgeTextColor = Qt::white;
    qreal m_rightInset = 0.0;
    qreal m_progress = 0.0;
    QString m_tip;
    bool m_hovered = false;
    bool m_pressed = false;

    void *m_nativeButton = nullptr;
};

} // namespace sc
