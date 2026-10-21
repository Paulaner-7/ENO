#include "Engines.h"

#include "core/Parallel.h"

#include <QJsonObject>
#include <QThread>
#include <QRegularExpression>
#include <QSet>

namespace sc::novels {

namespace {

QStringList paragraphs(const HtmlNode &node) {
    QStringList out;
    for (const auto &p : node.select(QStringLiteral("p")))
        if (auto text = p.textTrim(); !text.isEmpty())
            out.append(text);
    if (out.isEmpty())
        if (auto text = node.textTrim(); !text.isEmpty())
            out.append(text);
    return out;
}

// Many sites repeat the chapter title as the first paragraph ("Chapter 1:
// Chapter 1: Title"); the reader already shows it.
void dropRepeatedHeading(QList<Block> &blocks, const QString &title) {
    if (blocks.isEmpty() || title.isEmpty())
        return;
    const auto first = blocks.first().text.simplified().toLower();
    const auto t = title.simplified().toLower();
    if (first.size() < t.size() + 30 && (first.contains(t) || t.contains(first)))
        blocks.removeFirst();
}

void requireReadable(const QList<Block> &blocks, const QString &site) {
    if (blocks.isEmpty())
        throw Error(QStringLiteral("%1 returned no readable text for this chapter.").arg(site), 502);
}

} // namespace

// ---------------------------------------------------------------- Royal Road

QList<Book> RoyalRoad::listing(const QByteArray &html, int limit) {
    HtmlDocument doc(html);
    QList<Book> books;
    for (const auto &item : doc.select(QStringLiteral(".fiction-list-item"))) {
        const auto anchor = item.first(QStringLiteral("h2.fiction-title a[href]"));
        if (!anchor)
            continue;
        Book book;
        book.source = key();
        book.url = resolveUrl(m_config.base, anchor.attr("href"));
        static const QRegularExpression fictionPath(QStringLiteral(R"(^/fiction/(\d+))"));
        const auto m = fictionPath.match(QUrl(book.url).path());
        if (!m.hasMatch())
            continue;
        book.url = QStringLiteral("%1/fiction/%2").arg(m_config.base, m.captured(1));
        book.title = anchor.textTrim();
        book.cover = imgSrc(item.first(QStringLiteral("img")), m_config.base);
        if (book.cover.contains(QLatin1String("nocover")))
            book.cover.clear();
        book.description = paragraphs(item.first(QStringLiteral("[id^=\"description-\"]"))).join(QStringLiteral("\n\n"));
        for (const auto &tag : item.select(QStringLiteral("a.fiction-tag, .tags a")))
            if (auto text = tag.textTrim(); !text.isEmpty() && !book.genres.contains(text))
                book.genres.append(text);
        for (const auto &label : item.select(QStringLiteral(".tags span.label, .tags .label")))
            if (auto text = label.textTrim(); !text.isEmpty() && book.status.isEmpty())
                book.status = text;
        book.rating = item.first(QStringLiteral(".star[title]")).attr("title");
        for (const auto &span : item.select(QStringLiteral(".stats span"))) {
            const auto text = span.textTrim();
            if (text.endsWith(QLatin1String("Chapters"), Qt::CaseInsensitive))
                book.chapterCount = QString(text).remove(QLatin1Char(',')).section(QLatin1Char(' '), 0, 0).toInt();
        }
        books.append(book);
        if (books.size() >= limit)
            break;
    }
    return books;
}

QList<Book> RoyalRoad::search(const QString &query) {
    QUrl url(m_config.base + QStringLiteral("/fictions/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("title"), QString::fromUtf8(QUrl::toPercentEncoding(query)));
    url.setQuery(q);
    return listing(page(url.toString()).body, 30);
}

QList<Shelf> RoyalRoad::discover() {
    const QList<QPair<QString, QString>> feeds{{QStringLiteral("/fictions/best-rated"), QStringLiteral("Top rated")},
                                               {QStringLiteral("/fictions/rising-stars"), QStringLiteral("Rising stars")},
                                               {QStringLiteral("/fictions/trending"), QStringLiteral("Trending")},
                                               {QStringLiteral("/fictions/new-releases"), QStringLiteral("New arrivals")},
                                               {QStringLiteral("/fictions/complete"), QStringLiteral("Completed")}};
    QList<std::function<QList<Book>()>> jobs;
    for (const auto &[path, title] : feeds)
        jobs.append([this, path] { return listing(page(m_config.base + path).body, 20); });
    const auto results = parallel(jobs);
    QList<Shelf> shelves;
    QString firstError;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].ok() && !results[i].value->isEmpty())
            shelves.append({feeds[i].second, *results[i].value});
        else if (firstError.isEmpty())
            firstError = results[i].error;
    }
    if (shelves.isEmpty())
        throw Error(firstError.isEmpty() ? QStringLiteral("Royal Road returned no stories.") : firstError);
    return shelves;
}

Book RoyalRoad::book(const QString &url, QList<ChapterRef> &chapters) {
    static const QRegularExpression fictionPath(QStringLiteral(R"(^/fiction/(\d+))"));
    const auto m = fictionPath.match(QUrl(url).path());
    if (!m.hasMatch())
        throw Error(QStringLiteral("That is not a Royal Road story link."));
    const auto canonical = QStringLiteral("%1/fiction/%2").arg(m_config.base, m.captured(1));
    HtmlDocument doc(page(canonical).body);
    Book book;
    book.source = key();
    book.url = canonical;
    book.title = doc.first(QStringLiteral(".fic-header h1")).textTrim();
    if (book.title.isEmpty())
        throw Error(QStringLiteral("Royal Road returned a page SCANCORE could not read."), 502);
    book.cover = imgSrc(doc.first(QStringLiteral(".fic-header img.thumbnail, .fic-header img")), m_config.base);
    if (book.cover.contains(QLatin1String("nocover")))
        book.cover.clear();
    auto description = doc.first(QStringLiteral(".fiction-info .description .hidden-content"));
    if (!description)
        description = doc.first(QStringLiteral(".fiction-info .description"));
    book.description = paragraphs(description).join(QStringLiteral("\n\n"));
    QStringList authors;
    for (const auto &a : doc.select(QStringLiteral(".fic-header a[href^=\"/profile/\"]")))
        if (auto text = a.textTrim(); !text.isEmpty())
            authors.append(text);
    authors.removeDuplicates();
    book.author = authors.join(QStringLiteral(", "));
    for (const auto &tag : doc.select(QStringLiteral(".fiction-info .tags a")))
        if (auto text = tag.textTrim(); !text.isEmpty())
            book.genres.append(text);
    for (const auto &label : doc.select(QStringLiteral(".fiction-info .label")))
        if (const auto text = label.textTrim().toUpper();
            text == QLatin1String("ONGOING") || text == QLatin1String("COMPLETED") || text == QLatin1String("HIATUS") ||
            text == QLatin1String("STUB") || text == QLatin1String("DROPPED"))
            book.status = label.textTrim();
    book.rating = doc.first(QStringLiteral("meta[property=\"books:rating:value\"]")).attr("content");

    static const QRegularExpression chapterId(QStringLiteral(R"(/chapter/(\d+))"));
    int index = 0;
    for (const auto &a : doc.select(QStringLiteral("#chapters .chapter-row td:first-child a[href]"))) {
        const auto href = a.attr("href");
        if (!chapterId.match(href).hasMatch())
            continue;
        ++index;
        auto title = a.textTrim();
        chapters.append({resolveUrl(m_config.base, href), title.isEmpty() ? QStringLiteral("Chapter %1").arg(index) : title,
                         QString::number(index)});
    }
    book.chapterCount = int(chapters.size());
    return book;
}

QList<Block> RoyalRoad::chapter(const QString &url, QString &title) {
    HtmlDocument doc(page(url).body);
    auto content = doc.first(QStringLiteral(".chapter-inner.chapter-content, .chapter .chapter-content"));
    if (!content)
        throw Error(QStringLiteral("Royal Road returned no readable chapter text."), 502);
    // Royal Road hides decoy paragraphs with per-page classes declared in
    // <head> styles; drop them so they never reach the reader.
    static const QRegularExpression hiddenRule(QStringLiteral(R"((\.[\w-]+)\s*\{\s*display\s*:\s*none;[^}]*\})"));
    for (const auto &style : doc.select(QStringLiteral("head style"))) {
        auto it = hiddenRule.globalMatch(style.text());
        while (it.hasNext()) {
            const auto selector = it.next().captured(1);
            for (auto node : content.select(selector))
                node.remove();
        }
    }
    title = doc.first(QStringLiteral(".fic-header h1")).textTrim();
    const auto blocks = textBlocks(content);
    requireReadable(blocks, label());
    return blocks;
}

// ---------------------------------------------------------------- NovelFull family

QList<Book> NovelFull::parseSearch(const QByteArray &html) {
    HtmlDocument doc(html);
    QList<Book> books;
    QSet<QString> seen;
    auto add = [&](const HtmlNode &a, const HtmlNode &scope) {
        Book book;
        book.source = key();
        book.url = resolveUrl(origin(), a.attr("href"));
        if (book.url.isEmpty() || seen.contains(book.url))
            return;
        seen.insert(book.url);
        book.title = a.attr("title").trimmed();
        if (book.title.isEmpty())
            book.title = a.textTrim();
        book.cover = imgSrc(scope.first(QStringLiteral("img")), origin());
        book.author = scope.first(QStringLiteral(".author")).textTrim();
        for (const auto &g : scope.select(QStringLiteral("a[href*='/genre']")))
            if (auto text = g.textTrim(); !text.isEmpty())
                book.genres.append(text);
        books.append(book);
    };
    // fwn theme
    for (const auto &item : doc.select(QStringLiteral(".li-row .con, .col-content .li .con"))) {
        const auto a = item.first(QStringLiteral(".txt h3 a"));
        if (a)
            add(a, item);
    }
    // classic theme
    if (books.isEmpty())
        for (const auto &a : doc.select(QStringLiteral(
                 "#list-page .row h3[class*=\"title\"] > a, .list-truyen .row h3 > a, h3.novel-title > a")))
            add(a, a.closest(QStringLiteral(".row")));
    return books;
}

QList<Book> NovelFull::search(const QString &query) {
    QUrl url(origin() + m_config.options.value(QStringLiteral("searchPath")).toString(QStringLiteral("search")));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("keyword"), QString::fromUtf8(QUrl::toPercentEncoding(query)));
    url.setQuery(q);
    auto books = parseSearch(page(url.toString()).body);
    if (books.isEmpty() && m_config.options.value(QStringLiteral("searchPost")).toBool()) {
        const auto response = Http::postForm(QUrl(origin() + QStringLiteral("search")), {{"searchkey", query}},
                                             {{"Referer", origin().toUtf8()}});
        if (response.ok())
            books = parseSearch(response.body);
    }
    return books;
}

Book NovelFull::book(const QString &url, QList<ChapterRef> &chapters) {
    const auto response = page(url);
    HtmlDocument doc(response.body);
    Book book = doc.first(QStringLiteral(".m-desc, #idData")) ? fwnBook(doc, url, chapters)
                                                                : classicBook(doc, response.body, url, chapters);
    book.chapterCount = int(chapters.size());
    return book;
}

Book NovelFull::fwnBook(const HtmlDocument &doc, const QString &url, QList<ChapterRef> &chapters) {
    Book book;
    book.source = key();
    book.url = url;
    book.title = doc.first(QStringLiteral(".m-desc h1.tit, h1.tit")).textTrim();
    if (book.title.isEmpty())
        throw Error(QStringLiteral("%1 returned a page SCANCORE could not read.").arg(label()), 502);
    book.cover = imgSrc(doc.first(QStringLiteral(".m-imgtxt img, .pic img")), origin());
    book.author = doc.first(QStringLiteral(".m-imgtxt a[href*='/author'], .m-imgtxt a[href*='/a/']")).textTrim();
    for (const auto &a : doc.select(QStringLiteral(".m-imgtxt a[href*='/genre']")))
        if (auto text = a.textTrim(); !text.isEmpty() && !book.genres.contains(text))
            book.genres.append(text);
    for (const auto &item : doc.select(QStringLiteral(".m-imgtxt .item"))) {
        const auto text = item.textTrim();
        if (text.startsWith(QLatin1String("Status"), Qt::CaseInsensitive) || item.first(QStringLiteral(".glyphicon-time")))
            book.status = item.first(QStringLiteral(".right")).textTrim();
    }
    book.description = paragraphs(doc.first(QStringLiteral(".m-desc .txt .inner, .m-desc .inner, .m-desc .txt")))
                           .join(QStringLiteral("\n\n"));
    book.rating = doc.first(QStringLiteral(".m-desc .score p, .score .num")).textTrim();
    QSet<QString> seen;
    auto collect = [&](const HtmlNode &scope) {
        for (const auto &a : scope.select(QStringLiteral("li > a[href]"))) {
            const auto chapterUrl = resolveUrl(origin(), a.attr("href"));
            if (chapterUrl.isEmpty() || seen.contains(chapterUrl))
                continue;
            seen.insert(chapterUrl);
            auto title = a.attr("title").trimmed();
            if (title.isEmpty())
                title = a.textTrim();
            chapters.append({chapterUrl, title, QString::number(chapters.size() + 1)});
        }
    };
    collect(doc.first(QStringLiteral("#idData")));

    // Long books list 40 chapters per page; the rest come from the same
    // JSON endpoint the site's own pager calls.
    const auto pager = doc.first(QStringLiteral("#indexListPage"));
    // The endpoint accepts up to 200 per page, so ask for that and skip what
    // the book page already listed.
    const int totalChapters = pager.attr("data-total-chapters").toInt();
    const int pageSize = 200;
    const int totalPages = std::min((totalChapters + pageSize - 1) / pageSize, 100);
    if (totalChapters > chapters.size() && totalPages >= 1) {
        // Sequential with a short gap: these sites answer bursts with 429.
        for (int p = 1; p <= totalPages; ++p) {
            QUrl pageUrl(url);
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("ajax"), QStringLiteral("chapters"));
            q.addQueryItem(QStringLiteral("page"), QString::number(p));
            q.addQueryItem(QStringLiteral("pageSize"), QString::number(pageSize));
            pageUrl.setQuery(q);
            Response response;
            for (int attempt = 0; attempt < 4; ++attempt) {
                response = Http::get(pageUrl, {{"X-Requested-With", "XMLHttpRequest"}, {"Referer", url.toUtf8()}});
                if (response.status != 429)
                    break;
                QThread::msleep(1200 * (attempt + 1));
            }
            if (!response.ok())
                throw Error(QStringLiteral("%1 did not return the full chapter list (HTTP %2).").arg(label()).arg(response.status),
                            response.status);
            const auto html = response.json().object().value(QStringLiteral("html")).toString().toUtf8();
            HtmlDocument fragment(QByteArray("<ul>") + html + "</ul>");
            collect(fragment.root());
            QThread::msleep(120);
        }
    }
    return book;
}

Book NovelFull::classicBook(const HtmlDocument &doc, const QByteArray &raw, const QString &url,
                            QList<ChapterRef> &chapters) {
    Book book;
    book.source = key();
    book.url = url;
    book.title = doc.first(QStringLiteral("h3.title")).textTrim();
    if (book.title.isEmpty())
        throw Error(QStringLiteral("%1 returned a page SCANCORE could not read.").arg(label()), 502);
    book.cover = imgSrc(doc.first(QStringLiteral(".book img")), origin());
    QStringList authors;
    for (const auto &a : doc.select(
             QStringLiteral(".info a[href*='/a/'], .info a[href*='/au/'], .info a[href*='author']")))
        if (auto text = a.textTrim(); !text.isEmpty())
            authors.append(text);
    authors.removeDuplicates();
    book.author = authors.join(QStringLiteral(", "));
    for (const auto &li : doc.select(QStringLiteral(".info li, .info-meta li, .info > div"))) {
        const auto header = li.first(QStringLiteral("h3")).textTrim();
        if (header.contains(QLatin1String("Genre")) || header.contains(QLatin1String("Tag")))
            for (const auto &a : li.select(QStringLiteral("a")))
                if (auto text = a.textTrim(); !text.isEmpty())
                    book.genres.append(text);
        if (header.contains(QLatin1String("Status")))
            book.status = li.first(QStringLiteral("a")).textTrim();
    }
    book.description = paragraphs(doc.first(QStringLiteral(".desc-text"))).join(QStringLiteral("\n\n"));
    book.rating = doc.first(QStringLiteral("[itemprop=\"ratingValue\"]")).textTrim();

    QList<HtmlNode> anchors;
    std::unique_ptr<HtmlDocument> listDoc;
    if (m_config.options.value(QStringLiteral("chapterListOnPage")).toBool())
        anchors = doc.select(QStringLiteral("ul.list-chapter > li > a[href]"));
    if (anchors.isEmpty()) {
        const auto novelId = doc.first(QStringLiteral("#rating[data-novel-id]")).attr("data-novel-id");
        if (novelId.isEmpty()) {
            anchors = doc.select(QStringLiteral("ul.list-chapter > li > a[href]"));
        } else {
            auto ajax = m_config.options.value(QStringLiteral("chapterAjax")).toString();
            if (ajax.isEmpty())
                ajax = raw.contains("ajaxChapterOptionUrl") ? QStringLiteral("ajax-chapter-option")
                                                            : QStringLiteral("ajax/chapter-archive");
            listDoc = std::make_unique<HtmlDocument>(
                page(QStringLiteral("%1%2?novelId=%3").arg(origin(), ajax, novelId)).body);
            anchors = listDoc->select(QStringLiteral("ul.list-chapter > li > a[href], select > option[value]"));
        }
    }
    QSet<QString> seen;
    for (const auto &a : std::as_const(anchors)) {
        auto href = a.attr("href");
        if (href.isEmpty())
            href = a.attr("value");
        const auto chapterUrl = resolveUrl(origin(), href);
        if (chapterUrl.isEmpty() || seen.contains(chapterUrl))
            continue;
        seen.insert(chapterUrl);
        auto title = a.attr("title").trimmed();
        if (title.isEmpty())
            title = a.textTrim();
        chapters.append({chapterUrl, title, QString::number(chapters.size() + 1)});
    }
    return book;
}

QList<Block> NovelFull::chapter(const QString &url, QString &title) {
    HtmlDocument doc(page(url).body);
    auto blocks = doc.first(QStringLiteral(".m-read")) ? fwnChapter(doc, title) : classicChapter(doc, title);
    requireReadable(blocks, label());
    return blocks;
}

QList<Block> NovelFull::classicChapter(const HtmlDocument &doc, QString &title) {
    auto content = doc.first(QStringLiteral("#chr-content, #chapter-content"));
    if (!content)
        throw Error(QStringLiteral("%1 returned no readable chapter text.").arg(label()), 502);
    // Containers without paragraphs are ads and widgets on this theme.
    for (auto node : content.select(QStringLiteral("div, h1, h2, h3, h4, h5, h6")))
        if (!node.first(QStringLiteral("p")))
            node.remove();
    title = doc.first(QStringLiteral(".chr-title, a.chapter-title, .chapter-title")).textTrim();
    auto blocks = textBlocks(content);
    dropRepeatedHeading(blocks, title);
    return blocks;
}

QList<Block> NovelFull::fwnChapter(const HtmlDocument &doc, QString &title) {
    auto body = doc.first(QStringLiteral(".m-read"));
    // A page style hides randomised promotional lines; remove what it hides.
    static const QRegularExpression rule(QStringLiteral(R"(([^{}]+)\{[^}]*\})"));
    static const QRegularExpression nthLast(QStringLiteral(R"(p:nth-last-child\(\d+\))"));
    for (const auto &style : doc.select(QStringLiteral("style"))) {
        const auto css = style.text();
        if (!nthLast.match(css).hasMatch())
            continue;
        auto it = rule.globalMatch(css);
        while (it.hasNext()) {
            for (auto selector : it.next().captured(1).split(QLatin1Char(','))) {
                selector = selector.trimmed();
                if (selector.isEmpty() || nthLast.match(selector).hasMatch() || selector.startsWith(QLatin1Char('@')))
                    continue;
                for (auto node : body.select(selector))
                    node.remove();
            }
        }
    }
    for (auto node : body.select(QStringLiteral("h4, sub, img, .chapter-nav, .ads, script, .chapter-report-card")))
        node.remove();
    title = doc.first(QStringLiteral(".chapter-heading-text, .chapter-title, span.chapter")).textTrim();
    auto contents = body.first(QStringLiteral(".chapter-c, .txt"));
    auto blocks = textBlocks(contents ? contents : body);
    static const QRegularExpression promo(
        QStringLiteral(R"(([fF]ree\s?[Ww]eb\s?[nN]ovel|[nN]ovel\s?[fF]ull|[lL]ib\s?[rR]ead)[.\s]?([cC][oO0][mM]|[nN][eE][tT])?)"));
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [](const Block &b) { return b.text.size() < 120 && promo.match(b.text).hasMatch(); }),
                 blocks.end());
    dropRepeatedHeading(blocks, title);
    return blocks;
}

} // namespace sc::novels
