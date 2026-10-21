#include "ComicSource.h"

#include "Engines.h"

#include <QRegularExpression>

#include <algorithm>

namespace sc::comics {

QVariantMap Summary::toMap() const {
    return {{"title", title}, {"url", url},       {"cover", cover},   {"type", type},
            {"author", author}, {"genres", genres}, {"views", views}, {"source", source}};
}

QVariantMap Chapter::toMap() const {
    return {{"number", number}, {"title", title}, {"url", url},
            {"thumbnail", thumbnail}, {"slug", slug}, {"released", released}};
}

QVariantMap LatestItem::toMap() const {
    return {{"source", source},           {"sourceLabel", sourceLabel},   {"title", title},
            {"url", url},                 {"cover", cover},               {"chapterNumber", chapterNumber},
            {"chapterTitle", chapterTitle}, {"chapterUrl", chapterUrl}, {"publishedAt", publishedAt}};
}

bool Source::owns(const QUrl &url) const {
    const auto host = url.host().toLower();
    return std::any_of(m_config.hosts.begin(), m_config.hosts.end(),
                       [&](const QString &allowed) { return hostMatches(host, allowed); });
}

Headers Source::jsonHeaders() const {
    return {{"Accept", "application/json, text/plain, */*"},
            {"Origin", m_config.base.toUtf8()},
            {"Referer", (m_config.base + QLatin1Char('/')).toUtf8()}};
}

Headers Source::htmlHeaders() const {
    return {{"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
            {"Accept-Language", "en-US,en;q=0.9"},
            {"Referer", (m_config.base + QLatin1Char('/')).toUtf8()}};
}

QString Source::tagged(const QString &url) const {
    if (m_config.urlTag.isEmpty() || url.isEmpty())
        return url;
    return url.section(QLatin1Char('#'), 0, 0) + QLatin1Char('#') + m_config.urlTag;
}

QString Source::chapterLabel(const QString &number, const QString &title) const {
    return comics::chapterLabel(m_config.language, number, title);
}

std::unique_ptr<Source> createSource(const SourceConfig &config) {
    const auto &e = config.engine;
    if (e == QLatin1String("asura"))
        return std::make_unique<AsuraSource>(config);
    if (e == QLatin1String("webtoon"))
        return std::make_unique<WebtoonSource>(config);
    if (e == QLatin1String("mangadex"))
        return std::make_unique<MangaDexSource>(config);
    if (e == QLatin1String("flame"))
        return std::make_unique<FlameSource>(config);
    if (e == QLatin1String("hive"))
        return std::make_unique<HiveSource>(config);
    if (e == QLatin1String("iken"))
        return std::make_unique<IkenSource>(config);
    if (e == QLatin1String("qimanga"))
        return std::make_unique<QiMangaSource>(config);
    if (e == QLatin1String("madara"))
        return std::make_unique<MadaraSource>(config);
    if (e == QLatin1String("wp"))
        return std::make_unique<WpSource>(config);
    if (e == QLatin1String("mangaverse"))
        return std::make_unique<MangaverseSource>(config);
    if (e == QLatin1String("manhwaweb"))
        return std::make_unique<ManhwaWebSource>(config);
    if (e == QLatin1String("inmanga"))
        return std::make_unique<InMangaSource>(config);
    if (e == QLatin1String("leercapitulo"))
        return std::make_unique<LeerCapituloSource>(config);
    if (e == QLatin1String("mangaworld"))
        return std::make_unique<MangaWorldSource>(config);
    return nullptr;
}

QString str(const QJsonValue &value) {
    if (value.isString())
        return value.toString();
    if (value.isDouble()) {
        const double d = value.toDouble();
        if (d == double(qint64(d)))
            return QString::number(qint64(d));
        return QString::number(d);
    }
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

QString chapterNumberFrom(const QString &text) {
    // English and Spanish labels ("Chapter", "Ch.", "Capítulo", "Cap.",
    // "Episodio"), starting a word, so "Temporada 2 Capítulo 12" is 12.
    static const QRegularExpression labelled(
        QStringLiteral(R"((?<![\p{L}\p{N}])(?:chap(?:ter)?|ch|cap[ií]tulo|cap|episod(?:e|io)|ep)\.?\s*(\d+(?:\.\d+)?))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bare(QStringLiteral(R"(([\d]+(?:\.\d+)?))"));
    if (auto m = labelled.match(text.normalized(QString::NormalizationForm_C)); m.hasMatch())
        return m.captured(1);
    if (auto m = bare.match(text); m.hasMatch())
        return m.captured(1);
    return QStringLiteral("0");
}

QString chapterLabel(const QString &language, const QString &number, const QString &title) {
    const bool spanish = language == QLatin1String("es") || language.startsWith(QLatin1String("es-"));
    const bool italian = language == QLatin1String("it") || language.startsWith(QLatin1String("it-"));
    const auto word = spanish ? QString::fromUtf8("Capítulo") : italian ? QStringLiteral("Capitolo") : QStringLiteral("Chapter");
    return title.isEmpty() ? QStringLiteral("%1 %2").arg(word, number) : QStringLiteral("%1 %2: %3").arg(word, number, title);
}

void sortChapters(QList<Chapter> &chapters) {
    std::stable_sort(chapters.begin(), chapters.end(),
                     [](const Chapter &a, const Chapter &b) { return a.number.toDouble() < b.number.toDouble(); });
}

QString pathSegment(const QUrl &url, int index) {
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (index < 0)
        index += int(parts.size());
    return index >= 0 && index < parts.size() ? parts.at(index) : QString();
}

} // namespace sc::comics
