#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QUrl>
#include <QVariantMap>

namespace sc {

// image://colorwheel/<size> — the hue and saturation disc of the colour
// picker: hue runs counter-clockwise from 3 o'clock, saturation from the
// centre (white) to the rim, at full brightness.
class ColorWheelProvider : public QQuickImageProvider {
public:
    ColorWheelProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

// What Settings needs for themes and shortcuts beyond plain settings:
// background images kept with the app, .sct theme files, key sequences and
// the see-through window.
//
// A .sct file is JSON: {"format": "scancore-theme", "version": 2, "name",
// "colors": {"window", "accent"}, "window": {"opacity", "panels"},
// "background": {"type", "fit", "dim", "blur", "data" (base64)}, "look"}.
// The background travels inside it, so one file works on every system.
// "look" (version 2, core/ThemeLook.h) is the rest of the look: light or
// dark, navigation, spacing, corners, sizes, motion, arranged pages and the
// readers' colours. Version 1 files (colours and background) still open; a
// file without "colors.window" is a preset theme ("preset": true).
class Customize : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The window has an alpha channel, so the theme's window transparency
    // can show what is behind it. Decided at start (see main.cpp).
    Q_PROPERTY(bool translucentWindow READ translucentWindow CONSTANT)
    // SCANCORE can start itself again (not inside Flatpak or on phones).
    Q_PROPERTY(bool canRestart READ canRestart CONSTANT)
    // The interface language: "system", or one of `languages`.
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    // [{value, label}], each in its own language.
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)

public:
    static Customize *instance();
    static Customize *create(QQmlEngine *engine, QJSEngine *) {
        instance()->m_engine = engine;
        return instance();
    }

    // Loads the saved language's translation (main(), before the UI).
    static void applyLanguage();
    QString language() const;
    QVariantList languages() const;
    // Switches the language now: the whole interface is redrawn in it.
    Q_INVOKABLE void setLanguage(const QString &code);

    // Whether the saved theme asks for a see-through window; main() then
    // gives windows an alpha channel before creating any.
    static bool savedThemeIsTranslucent();
    static void setTranslucentWindow(bool on);

    bool translucentWindow() const;
    bool canRestart() const;

    // The shortcut for a key press, as Shortcut.sequence takes it ("Ctrl+K");
    // empty for a modifier alone.
    Q_INVOKABLE QString keySequence(int key, int modifiers) const;
    // How a shortcut is written on this system ("⌘K" on macOS).
    Q_INVOKABLE QString keyText(const QString &sequence) const;
    // Whether two shortcuts are the same keys, however they are written.
    Q_INVOKABLE bool sameKeys(const QString &a, const QString &b) const;

    // Copies an image or GIF into the app's theme folder. {ok, url,
    // animated} or {ok: false, error}.
    Q_INVOKABLE QVariantMap useBackground(const QUrl &file);
    // theme: the customTheme setting (empty for a preset theme) plus
    // "accent" (#rrggbb, custom accents) and "look" (see above).
    Q_INVOKABLE sc::Task *exportTheme(const QVariantMap &theme, const QUrl &file);
    // A theme map ready for the customTheme setting, plus "accent", "look"
    // and "preset" (Router.applyTheme applies it).
    Q_INVOKABLE sc::Task *importTheme(const QUrl &file);
    // The themes people have shared in the SCANCORE repository (themes/):
    // [{id, name, author, description, window, accent, preview, file}].
    Q_INVOKABLE sc::Task *gallery();
    // Downloads one from the gallery; the result is what importTheme gives.
    Q_INVOKABLE sc::Task *installTheme(const QString &url);

    Q_INVOKABLE void restart();

signals:
    void languageChanged();

private:
    explicit Customize(QObject *parent = nullptr) : QObject(parent) {}

    QQmlEngine *m_engine = nullptr;
};

} // namespace sc
