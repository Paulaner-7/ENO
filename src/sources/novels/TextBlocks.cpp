#include "NovelSource.h"

#ifdef SCANCORE_BUNDLED_SOURCES
#include "sources/bundled/novels/Engines.h"
#endif

#include <QRegularExpression>
#include <QSet>

namespace sc::novels {

QVariantMap Book::toMap() const {
    return {{"source", source}, {"url", url},       {"sourceUrl", url},       {"title", title},
            {"author", author}, {"cover", cover},   {"description", description}, {"status", status},
            {"rating", rating}, {"genres", genres}, {"chapterCount", chapterCount}};
}

bool Source::owns(const QUrl &url) const {
    const auto host = url.host().toLower();
    return std::any_of(m_config.hosts.begin(), m_config.hosts.end(),
                       [&](const QString &allowed) { return hostMatches(host, allowed); });
}

Response Source::page(const QString &url) {
    return Http::getOk(QUrl(url),
                       {{"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
                        {"Accept-Language", "en-US,en;q=0.9"},
                        {"Referer", origin().toUtf8()}},
                       20000);
}

std::unique_ptr<Source> createSource(const SourceConfig &config) {
#ifndef SCANCORE_BUNDLED_SOURCES
    Q_UNUSED(config)
    return nullptr;
#else
    if (config.engine == QLatin1String("royalroad"))
        return std::make_unique<RoyalRoad>(config);
    if (config.engine == QLatin1String("novelfull") || config.engine == QLatin1String("freewebnovel"))
        return std::make_unique<NovelFull>(config);
    if (config.engine == QLatin1String("wtrlab"))
        return std::make_unique<WtrLab>(config);
    return nullptr;
#endif
}

QString imgSrc(const HtmlNode &img, const QString &base) {
    for (const char *attr : {"data-src", "data-lazy-src", "data-original", "src"}) {
        const auto value = img.attr(attr).trimmed();
        if (!value.isEmpty() && !value.startsWith(QLatin1String("data:")))
            return resolveUrl(base, value);
    }
    return {};
}

namespace {

struct Marks {
    bool italic = false;
    bool bold = false;
    bool underline = false;
    bool strike = false;
    bool operator==(const Marks &) const = default;
};

struct Run {
    QString text;
    Marks marks;
};

const QSet<QString> kBoundaries{QStringLiteral("p"),          QStringLiteral("div"),     QStringLiteral("li"),
                                QStringLiteral("blockquote"), QStringLiteral("h1"),      QStringLiteral("h2"),
                                QStringLiteral("h3"),         QStringLiteral("h4"),      QStringLiteral("h5"),
                                QStringLiteral("h6"),         QStringLiteral("section"), QStringLiteral("article"),
                                QStringLiteral("table"),      QStringLiteral("tr")};
const QSet<QString> kOmitted{QStringLiteral("script"), QStringLiteral("style"),  QStringLiteral("noscript"),
                             QStringLiteral("iframe"), QStringLiteral("form"),   QStringLiteral("object"),
                             QStringLiteral("embed"),  QStringLiteral("svg"),    QStringLiteral("math"),
                             QStringLiteral("button"), QStringLiteral("ins"),    QStringLiteral("img")};

class Builder {
public:
    QList<Block> blocks;

    void walk(const HtmlNode &node, Marks marks, bool insideElement) {
        if (node.isText()) {
            const auto data = node.text();
            if (insideElement) {
                append(data, marks);
                return;
            }
            const auto lines = data.split(QRegularExpression(QStringLiteral("\\n+")));
            for (int i = 0; i < lines.size(); ++i) {
                if (i)
                    finish();
                append(lines[i], marks);
            }
            return;
        }
        if (!node.isElement())
            return;
        const auto tag = node.tag();
        const auto style = node.attr("style");
        static const QRegularExpression hiddenStyle(QStringLiteral(R"((display\s*:\s*none|visibility\s*:\s*hidden))"),
                                                    QRegularExpression::CaseInsensitiveOption);
        if (kOmitted.contains(tag) || node.hasAttr("hidden") || node.attr("aria-hidden") == QLatin1String("true") ||
            hiddenStyle.match(style).hasMatch())
            return;
        if (tag == QLatin1String("br")) {
            finish();
            return;
        }
        if (tag == QLatin1String("hr")) {
            finish();
            blocks.append({QStringLiteral("break"), QString(), QString()});
            return;
        }
        const bool boundary = kBoundaries.contains(tag);
        const auto previousKind = m_kind;
        if (boundary) {
            finish();
            if (tag.size() == 2 && tag[0] == QLatin1Char('h') && tag[1].isDigit())
                m_kind = QStringLiteral("heading");
            else if (tag == QLatin1String("blockquote"))
                m_kind = QStringLiteral("quote");
        }
        Marks next = marks;
        static const QRegularExpression italic(QStringLiteral(R"(font-style\s*:\s*(italic|oblique))"),
                                               QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression bold(QStringLiteral(R"(font-weight\s*:\s*(bold|[7-9]00))"),
                                             QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression underline(QStringLiteral(R"(text-decoration(-line)?\s*:[^;]*underline)"),
                                                  QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression strike(QStringLiteral(R"(text-decoration(-line)?\s*:[^;]*line-through)"),
                                               QRegularExpression::CaseInsensitiveOption);
        if (tag == QLatin1String("i") || tag == QLatin1String("em") || tag == QLatin1String("cite") ||
            italic.match(style).hasMatch())
            next.italic = true;
        if (tag == QLatin1String("b") || tag == QLatin1String("strong") || bold.match(style).hasMatch())
            next.bold = true;
        if (tag == QLatin1String("u") || underline.match(style).hasMatch())
            next.underline = true;
        if (tag == QLatin1String("s") || tag == QLatin1String("del") || tag == QLatin1String("strike") ||
            strike.match(style).hasMatch())
            next.strike = true;
        for (const auto &child : node.childNodes())
            walk(child, next, tag != QLatin1String("pre"));
        if (boundary) {
            finish();
            m_kind = previousKind;
        }
    }

    void finish() {
        while (!m_runs.isEmpty()) {
            auto &first = m_runs.first().text;
            while (!first.isEmpty() && first.front().isSpace())
                first.remove(0, 1);
            if (!first.isEmpty())
                break;
            m_runs.removeFirst();
        }
        while (!m_runs.isEmpty()) {
            auto &last = m_runs.last().text;
            while (!last.isEmpty() && last.back().isSpace())
                last.chop(1);
            if (!last.isEmpty())
                break;
            m_runs.removeLast();
        }
        if (m_runs.isEmpty())
            return;
        QString text;
        QString html;
        for (const auto &run : std::as_const(m_runs)) {
            text += run.text;
            QString piece = run.text.toHtmlEscaped();
            if (run.marks.strike)
                piece = QStringLiteral("<s>%1</s>").arg(piece);
            if (run.marks.underline)
                piece = QStringLiteral("<u>%1</u>").arg(piece);
            if (run.marks.italic)
                piece = QStringLiteral("<i>%1</i>").arg(piece);
            if (run.marks.bold)
                piece = QStringLiteral("<b>%1</b>").arg(piece);
            html += piece;
        }
        if (!text.isEmpty())
            blocks.append({m_kind, text, html});
        m_runs.clear();
    }

private:
    void append(QString text, const Marks &marks) {
        static const QRegularExpression ws(QStringLiteral("\\s+"));
        text.replace(ws, QStringLiteral(" "));
        text.replace(QChar(0xA0), QLatin1Char(' '));
        if (text.isEmpty())
            return;
        if (!m_runs.isEmpty() && m_runs.last().text.endsWith(QLatin1Char(' ')) && text.startsWith(QLatin1Char(' ')))
            text.remove(0, 1);
        if (text.isEmpty())
            return;
        if (!m_runs.isEmpty() && m_runs.last().marks == marks)
            m_runs.last().text += text;
        else
            m_runs.append({text, marks});
    }

    QList<Run> m_runs;
    QString m_kind = QStringLiteral("paragraph");
};

} // namespace

QList<Block> textBlocks(const HtmlNode &container) {
    Builder builder;
    for (const auto &child : container.childNodes())
        builder.walk(child, {}, false);
    builder.finish();
    // Collapse runs of breaks and drop leading/trailing ones.
    QList<Block> out;
    for (const auto &block : std::as_const(builder.blocks)) {
        if (block.kind == QLatin1String("break") && (out.isEmpty() || out.last().kind == QLatin1String("break")))
            continue;
        out.append(block);
    }
    while (!out.isEmpty() && out.last().kind == QLatin1String("break"))
        out.removeLast();
    return out;
}

} // namespace sc::novels
