#include "AnimeSource.h"

#include "Host.h"
#include "sources/comics/ComicSource.h"

#include <QJsonDocument>
#include <QUrl>

namespace sc::ext {

AnimeSource::AnimeSource(const SourceEntry &entry) : m_entry(entry), m_config{entry.lang} {}

QString AnimeSource::unwrap(const QString &url) const {
    const auto wrapped = unwrapUrl(url);
    if (!wrapped || wrapped->key != key())
        throw Error(QStringLiteral("That link does not belong to %1.").arg(label()));
    return wrapped->url;
}

QJsonValue AnimeSource::call(const QString &method, QJsonObject params) {
    if (!m_entry.blocked.isEmpty())
        throw Error(m_entry.blocked);
    params.insert(QStringLiteral("source"), m_entry.id);
    params.insert(QStringLiteral("pkg"), m_entry.pkg);
    params.insert(QStringLiteral("apk"), m_entry.path);
    if (!m_entry.signer.isEmpty())
        params.insert(QStringLiteral("signer"), m_entry.signer);
    try {
        const auto result = requireHost(m_entry.kind)->call(method, params);
        Manager::recordSourceResult(key(), {});
        return result;
    } catch (const Error &e) {
        // Cancelled, or an id from before the host restarted: not the
        // source's fault.
        if (e.status() != 499 && e.status() != 410)
            Manager::recordSourceResult(key(), e.message());
        throw;
    }
}

AnimeItem AnimeSource::item(const QJsonObject &o) const {
    AnimeItem a;
    a.url = wrapUrl(key(), o.value("url").toString());
    a.title = o.value("title").toString().trimmed();
    a.cover = o.value("cover").toString();
    a.seasons = o.value("fetchType").toString() == QLatin1String("seasons");
    a.season = o.value("season").toDouble(-1);
    return a;
}

AnimeListing AnimeSource::browse(const QString &listing, int page, const QString &query, const QJsonArray &filters) {
    QJsonObject params{{"page", std::max(1, page)}};
    if (listing == QLatin1String("search")) {
        params.insert("query", query);
        params.insert("filters", filters);
    }
    const auto method = listing == QLatin1String("latest") ? QStringLiteral("latest")
                        : listing == QLatin1String("popular") ? QStringLiteral("popular")
                                                               : QStringLiteral("search");
    const auto o = call(method, params).toObject();
    AnimeListing out;
    for (const auto &value : o.value("items").toArray()) {
        const auto a = item(value.toObject());
        if (!a.title.isEmpty() && a.url.size() > key().size() + 1)
            out.items.append(a);
    }
    out.hasNextPage = o.value("hasNextPage").toBool();
    return out;
}

QList<AnimeItem> AnimeSource::search(const QString &query) { return browse(QStringLiteral("search"), 1, query).items; }

QJsonObject AnimeSource::details(const QString &url, const QString &title, bool seasons) {
    return call(QStringLiteral("details"), {{"url", unwrap(url)},
                                            {"title", title},
                                            {"fetchType", seasons ? QStringLiteral("seasons") : QStringLiteral("episodes")}})
        .toObject();
}

QJsonArray AnimeSource::episodes(const QString &url, const QString &title) {
    const auto result = call(QStringLiteral("episodes"), {{"url", unwrap(url)}, {"title", title}}).toArray();
    QJsonArray out;
    // Extensions list the newest first; SCANCORE keeps the oldest first.
    for (auto it = result.end(); it != result.begin();) {
        auto e = (--it)->toObject();
        const auto link = e.value("url").toString();
        if (link.isEmpty())
            continue;
        e.insert("url", wrapUrl(key(), link));
        auto number = e.value("number").toDouble(-1);
        if (number < 0)
            if (const auto parsed = comics::chapterNumberFrom(e.value("name").toString()); !parsed.isEmpty())
                number = parsed.toDouble();
        e.insert("number", number);
        out.append(e);
    }
    return out;
}

QList<AnimeItem> AnimeSource::seasons(const QString &url, const QString &title) {
    QList<AnimeItem> out;
    for (const auto &value : call(QStringLiteral("seasons"), {{"url", unwrap(url)}, {"title", title}}).toArray())
        out.append(item(value.toObject()));
    return out;
}

QJsonArray AnimeSource::hosters(const QString &episodeUrl, const QString &name) {
    return call(QStringLiteral("hosters"), {{"url", unwrap(episodeUrl)}, {"name", name}}).toArray();
}

QJsonArray AnimeSource::hosterVideos(const QString &hosterId) {
    return call(QStringLiteral("hosterVideos"), {{"hoster", hosterId}}).toArray();
}

QJsonObject AnimeSource::resolveVideo(const QString &videoId) {
    return call(QStringLiteral("resolveVideo"), {{"video", videoId}}).toObject();
}

QJsonArray AnimeSource::filters() { return call(QStringLiteral("filters"), {}).toArray(); }

QJsonArray AnimeSource::preferences() { return call(QStringLiteral("preferences"), {}).toArray(); }

void AnimeSource::setPreference(const QString &prefKey, const QJsonValue &value) {
    call(QStringLiteral("setPreference"), {{"key", prefKey}, {"value", value}});
    Manager::setPreference(key(), prefKey, QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)));
}

void AnimeSource::fetchImage(const QString &url, const QString &path) {
    call(QStringLiteral("fetchImage"), {{"page", QJsonObject{{"imageUrl", url}}}, {"target", path}});
}

QString AnimeSource::webUrl(const QString &url) const {
    const auto path = unwrap(url);
    if (path.startsWith(QLatin1String("http")))
        return path;
    const auto base = m_entry.baseUrl.endsWith(QLatin1Char('/')) ? m_entry.baseUrl : m_entry.baseUrl + QLatin1Char('/');
    return QUrl(base).resolved(QUrl(path)).toString();
}

} // namespace sc::ext
