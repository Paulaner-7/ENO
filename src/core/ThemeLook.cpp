#include "core/ThemeLook.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace sc::theme {

namespace {

bool isWord(const QString &text, int max) {
    static const QRegularExpression word(QStringLiteral("^[a-z0-9][a-z0-9_-]*$"));
    return !text.isEmpty() && text.size() <= max && word.match(text).hasMatch();
}

// "#rgb" or "#rrggbb" as "#rrggbb"; empty for anything else (core has no
// QColor).
QString hexColour(const QString &text) {
    static const QRegularExpression hex(QStringLiteral("^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$"));
    if (!hex.match(text).hasMatch())
        return {};
    if (text.size() == 7)
        return text.toLower();
    QString out(QStringLiteral("#"));
    for (int i = 1; i < 4; ++i)
        out += QString(2, text.at(i).toLower());
    return out;
}

// Settings with a fixed set of values.
const QHash<QString, QStringList> &choices() {
    static const QHash<QString, QStringList> c{
        {"navStyle", {"sidebar", "floating", "rail", "dock"}},
        {"navSide", {"left", "right"}},
        {"dockSide", {"bottom", "top"}},
        {"pageSpacing", {"compact", "default", "spacious"}},
        {"roundness", {"square", "subtle", "default", "round"}},
        {"coverSize", {"small", "default", "large"}},
        {"motionSpeed", {"relaxed", "default", "snappy"}},
    };
    return c;
}

const QStringList kColours{"readerRailUnread", "readerRailRead", "readerRailCurrent", "novelCustomBg", "novelCustomText"};
const QStringList kPages{"home", "comics", "comics-top", "novels", "novels-top", "anime"};
const QStringList kModes{"system", "light", "dark", "black", "custom"};

QJsonObject sanitizeSettings(const QJsonObject &in) {
    QJsonObject out;
    for (auto it = choices().begin(); it != choices().end(); ++it) {
        const auto value = in.value(it.key()).toString();
        if (it.value().contains(value))
            out.insert(it.key(), value);
    }
    for (const auto &key : kColours) {
        const auto value = hexColour(in.value(key).toString());
        if (!value.isEmpty())
            out.insert(key, value);
    }
    if (in.value("textScale").isDouble())
        out.insert("textScale", std::clamp(in.value("textScale").toDouble(), 0.8, 1.4));
    if (in.value("readerProgressWidth").isDouble())
        out.insert("readerProgressWidth", std::clamp(int(std::lround(in.value("readerProgressWidth").toDouble())), 2, 12));
    const auto novelTheme = in.value("novelTheme").toString();
    if (isWord(novelTheme, 24))
        out.insert("novelTheme", novelTheme);
    return out;
}

// One page's arrangement (PageLayout.qml): order, hidden, look.
QJsonObject sanitizePage(const QJsonObject &in) {
    QJsonObject out;
    QJsonArray order;
    for (const auto &value : in.value("order").toArray()) {
        const auto key = value.toString();
        if (isWord(key, 40) && order.size() < 64 && !order.contains(key))
            order.append(key);
    }
    if (!order.isEmpty())
        out.insert("order", order);
    QJsonObject hidden;
    const auto hiddenIn = in.value("hidden").toObject();
    for (auto it = hiddenIn.begin(); it != hiddenIn.end() && hidden.size() < 64; ++it)
        if (isWord(it.key(), 40) && it.value().toBool())
            hidden.insert(it.key(), true);
    if (!hidden.isEmpty())
        out.insert("hidden", hidden);
    QJsonObject looks;
    const auto looksIn = in.value("look").toObject();
    for (auto it = looksIn.begin(); it != looksIn.end() && looks.size() < 64; ++it) {
        if (!isWord(it.key(), 40))
            continue;
        const auto l = it.value().toObject();
        QJsonObject look;
        for (const auto *field : {"style", "size"})
            if (isWord(l.value(field).toString(), 20))
                look.insert(field, l.value(field).toString());
        if (l.value("count").isDouble())
            look.insert("count", std::clamp(int(l.value("count").toDouble()), 0, 200));
        for (const auto *field : {"subtitles", "progress"})
            if (l.value(field).isBool())
                look.insert(field, l.value(field).toBool());
        if (!look.isEmpty())
            looks.insert(it.key(), look);
    }
    if (!looks.isEmpty())
        out.insert("look", looks);
    return out;
}

} // namespace

QJsonObject sanitizeLook(const QJsonObject &look) {
    QJsonObject out;
    const auto mode = look.value("mode").toString();
    if (kModes.contains(mode))
        out.insert("mode", mode);
    const auto accentName = look.value("accentName").toString();
    if (isWord(accentName, 20))
        out.insert("accentName", accentName);
    const auto settings = sanitizeSettings(look.value("settings").toObject());
    if (!settings.isEmpty())
        out.insert("settings", settings);
    QJsonObject pages;
    const auto pagesIn = look.value("pages").toObject();
    for (const auto &name : kPages) {
        const auto page = sanitizePage(pagesIn.value(name).toObject());
        if (!page.isEmpty())
            pages.insert(name, page);
    }
    if (!pages.isEmpty())
        out.insert("pages", pages);
    return out;
}

} // namespace sc::theme
