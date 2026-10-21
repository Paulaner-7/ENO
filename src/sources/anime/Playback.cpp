#include "Playback.h"

#include <QHash>

#include <cmath>

namespace sc::anime {

QVariantMap videoMap(const QJsonObject &video) {
    static const QHash<QString, QString> types{
        {"Opening", "op"}, {"Ending", "ed"}, {"Recap", "recap"}, {"MixedOp", "mixed-op"}};
    QVariantList skips;
    for (const auto &value : video.value("timestamps").toArray()) {
        const auto t = value.toObject();
        if (const auto type = types.value(t.value("type").toString()); !type.isEmpty())
            skips.append(QVariantMap{{"type", type}, {"start", t.value("start").toDouble()}, {"end", t.value("end").toDouble()}});
    }
    auto map = video.toVariantMap();
    map.insert(QStringLiteral("skips"), skips);
    return map;
}

QVariantList videoList(const QJsonArray &videos) {
    QVariantList out;
    out.reserve(videos.size());
    for (const auto &v : videos)
        out.append(videoMap(v.toObject()));
    return out;
}

QVariant bestVideo(const QVariantList &hosters) {
    enum Pass { Preferred, WithAddress, Unresolved };
    for (const auto pass : {Preferred, WithAddress, Unresolved})
        for (int h = 0; h < hosters.size(); ++h) {
            const auto hoster = hosters[h].toMap();
            if (hoster.value("state").toString() != QLatin1String("ready"))
                continue;
            const auto videos = hoster.value("videos").toList();
            for (int v = 0; v < videos.size(); ++v) {
                const auto video = videos[v].toMap();
                if (video.value("localServer").toBool())
                    continue;
                const bool hasUrl = !video.value("url").toString().isEmpty();
                const bool usable = pass == Preferred     ? video.value("preferred").toBool()
                                    : pass == WithAddress ? hasUrl
                                                          : !video.value("initialized").toBool();
                if (usable)
                    return QVariantMap{{"hoster", h}, {"video", v}};
            }
        }
    return {};
}

QString episodeNumbered(const QJsonArray &episodes, double number) {
    bool numbered = false;
    for (const auto &value : episodes) {
        const auto e = value.toObject();
        const double n = e.value("number").toDouble(-1);
        numbered = numbered || n >= 0;
        if (n >= 0 && std::abs(n - number) < 0.001)
            return e.value("url").toString();
    }
    const int index = int(number) - 1;
    if (!numbered && index >= 0 && index < episodes.size())
        return episodes[index].toObject().value("url").toString();
    return {};
}

} // namespace sc::anime
