#include "ComicSource.h"

#ifdef SCANCORE_BUNDLED_SOURCES
#include "sources/bundled/comics/Engines.h"
#endif

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace sc::comics {

QVariantMap Summary::toMap() const {
    return {{"title", title}, {"url", url},       {"cover", cover},   {"type", type},
            {"author", author}, {"genres", genres}, {"views", views}, {"source", source}};
}

QVariantMap Chapter::toMap() const {
    return {{"number", number}, {"title", title},       {"url", url},          {"thumbnail", thumbnail},
            {"slug", slug},     {"released", released}, {"scanlator", scanlator}};
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
#ifndef SCANCORE_BUNDLED_SOURCES
    // Built without the bundled sources: extensions and files only.
    Q_UNUSED(config)
    return nullptr;
#else
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
    if (e == QLatin1String("dongman"))
        return std::make_unique<DongmanSource>(config);
    if (e == QLatin1String("kakaopage"))
        return std::make_unique<KakaoPageSource>(config);
    if (e == QLatin1String("naver"))
        return std::make_unique<NaverSource>(config);
    if (e == QLatin1String("zonatmo"))
        return std::make_unique<ZonaTmoSource>(config);
    if (e == QLatin1String("olympus"))
        return std::make_unique<OlympusSource>(config);
    if (e == QLatin1String("zerocomics"))
        return std::make_unique<ZeroComicsSource>(config);
    return nullptr;
#endif
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
    // "%1" is the number, in the source's language (English for the rest).
    static const QHash<QString, QString> words{
        {QStringLiteral("es"), QString::fromUtf8("Capítulo %1")}, {QStringLiteral("pt"), QString::fromUtf8("Capítulo %1")},
        {QStringLiteral("fr"), QStringLiteral("Chapitre %1")},    {QStringLiteral("it"), QStringLiteral("Capitolo %1")},
        {QStringLiteral("de"), QStringLiteral("Kapitel %1")},     {QStringLiteral("pl"), QString::fromUtf8("Rozdział %1")},
        {QStringLiteral("ru"), QString::fromUtf8("Глава %1")},    {QStringLiteral("tr"), QString::fromUtf8("Bölüm %1")},
        {QStringLiteral("vi"), QString::fromUtf8("Chương %1")},   {QStringLiteral("th"), QString::fromUtf8("ตอนที่ %1")},
        {QStringLiteral("ar"), QString::fromUtf8("الفصل %1")},    {QStringLiteral("ko"), QString::fromUtf8("%1화")},
        {QStringLiteral("ja"), QString::fromUtf8("第%1話")},      {QStringLiteral("zh"), QString::fromUtf8("第%1话")},
    };
    const auto base = language.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
    const auto label = words.value(base, QStringLiteral("Chapter %1")).arg(number);
    return title.isEmpty() ? label : QStringLiteral("%1: %2").arg(label, title);
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
