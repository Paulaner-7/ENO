#include "PluginSource.h"

#include "core/Database.h"
#include "core/Html.h"
#include "extensions/Host.h"

#include <QJsonDocument>

namespace sc::plugins {

namespace {

QString text(const QJsonValue &value) { return value.isString() ? value.toString().trimmed() : QString(); }

// LNReader's statuses as SCANCORE shows them.
QString statusName(const QString &status) {
    if (status == QLatin1String("Publishing Finished"))
        return QStringLiteral("Completed");
    return status == QLatin1String("Unknown") ? QString() : status;
}

} // namespace

PluginSource::PluginSource(const ext::SourceEntry &entry)
    : novels::Source([&] {
          novels::SourceConfig config;
          config.key = entry.key;
          config.label = entry.name;
          config.engine = QStringLiteral("lnreader");
          config.base = entry.baseUrl;
          config.language = entry.lang;
          config.enabled = entry.enabled && entry.blocked.isEmpty();
          config.disabledReason = entry.blocked;
          return config;
      }()),
      m_entry(entry), m_plugin{entry.key, entry.id, entry.path} {}

QJsonValue PluginSource::call(const QString &method, const QJsonArray &args) {
    if (!m_entry.blocked.isEmpty())
        throw Error(m_entry.blocked);
    try {
        const auto result = Runtime::call(m_plugin, method, args);
        ext::Manager::recordSourceResult(key(), {});
        return result;
    } catch (const Error &e) {
        if (e.status() != 499)
            ext::Manager::recordSourceResult(key(), e.message());
        throw;
    }
}

QString PluginSource::unwrap(const QString &url) const {
    const auto wrapped = ext::unwrapUrl(url);
    return wrapped && wrapped->key == key() ? wrapped->url : url;
}

novels::Book PluginSource::bookFrom(const QJsonObject &item) const {
    novels::Book book;
    book.source = key();
    book.url = ext::wrapUrl(key(), text(item.value("path")));
    book.title = text(item.value("name"));
    book.cover = text(item.value("cover"));
    return book;
}

QList<novels::Book> PluginSource::search(const QString &query) {
    return browse(QStringLiteral("search"), 1, query, {}).items;
}

novels::Listing PluginSource::browse(const QString &listing, int page, const QString &query, const QJsonArray &filters) {
    QJsonValue result;
    if (listing == QLatin1String("search") && !query.trimmed().isEmpty()) {
        result = call(QStringLiteral("searchNovels"), {query.trimmed(), page});
    } else {
        // Filters shape the plugin's lists (LNReader passes them to
        // popularNovels, not to search).
        // LNReader always passes them, the plugin's own defaults when the
        // reader chose none; plugins read them unchecked.
        const auto declared = Runtime::info(m_plugin).value("filters").toObject();
        QJsonObject options{{"showLatestNovels", listing == QLatin1String("latest")},
                            {"filters", filters.isEmpty() ? declared : fromFilterSheet(declared, filters)}};
        result = call(QStringLiteral("popularNovels"), {page, options});
    }
    novels::Listing out;
    for (const auto &value : result.toArray()) {
        const auto book = bookFrom(value.toObject());
        if (!book.title.isEmpty() && !text(value.toObject().value("path")).isEmpty())
            out.items.append(book);
    }
    // Plugins say nothing about further pages; an empty page ends the list.
    out.hasNextPage = !out.items.isEmpty();
    return out;
}

novels::Book PluginSource::book(const QString &url, QList<novels::ChapterRef> &chapters) {
    const auto path = unwrap(url);
    const auto o = call(QStringLiteral("parseNovel"), {path}).toObject();
    novels::Book book;
    book.source = key();
    book.url = url;
    book.title = text(o.value("name"));
    book.cover = text(o.value("cover"));
    book.author = text(o.value("author"));
    book.description = text(o.value("summary"));
    book.status = statusName(text(o.value("status")));
    for (const auto &genre : text(o.value("genres")).split(QLatin1Char(','), Qt::SkipEmptyParts))
        book.genres.append(genre.trimmed());
    const auto add = [&](const QJsonArray &list) {
        for (const auto &value : list) {
            const auto c = value.toObject();
            const auto chapterPath = text(c.value("path"));
            if (chapterPath.isEmpty())
                continue;
            novels::ChapterRef ref;
            ref.url = ext::wrapUrl(key(), chapterPath);
            ref.title = text(c.value("name"));
            if (c.value("chapterNumber").isDouble())
                ref.number = QString::number(c.value("chapterNumber").toDouble());
            chapters.append(ref);
        }
    };
    add(o.value("chapters").toArray());
    // Long books list their chapters in pages (parsePage).
    const int pages = o.value("totalPages").toInt();
    if (pages > 1 && Runtime::info(m_plugin).value("hasParsePage").toBool()) {
        for (int page = chapters.isEmpty() ? 1 : 2; page <= std::min(pages, 200); ++page)
            add(call(QStringLiteral("parsePage"), {path, QString::number(page)}).toObject().value("chapters").toArray());
    }
    book.chapterCount = int(chapters.size());
    return book;
}

QList<novels::Block> PluginSource::chapter(const QString &url, QString &title) {
    Q_UNUSED(title)
    const auto html = call(QStringLiteral("parseChapter"), {unwrap(url)}).toString();
    if (html.trimmed().isEmpty())
        throw Error(QStringLiteral("The plugin found no text in this chapter."));
    HtmlDocument document(html);
    return novels::textBlocks(document.body());
}

QString PluginSource::webUrl(const QString &url, bool novel) {
    const auto path = unwrap(url);
    if (path.startsWith(QLatin1String("http")))
        return path;
    if (Runtime::info(m_plugin).value("hasResolveUrl").toBool()) {
        const auto resolved = call(QStringLiteral("resolveUrl"), {path, novel}).toString();
        if (!resolved.isEmpty())
            return resolved;
    }
    return QUrl(m_entry.baseUrl.endsWith(QLatin1Char('/')) ? m_entry.baseUrl : m_entry.baseUrl + QLatin1Char('/')).resolved(QUrl(path)).toString();
}

QJsonArray PluginSource::filters() { return toFilterSheet(Runtime::info(m_plugin).value("filters").toObject()); }

QString PluginSource::imageReferer() const { return m_entry.baseUrl.isEmpty() ? QString() : m_entry.baseUrl; }

// ---------------------------------------------------------------- filters
QJsonArray PluginSource::toFilterSheet(const QJsonObject &filters) {
    QJsonArray out;
    for (auto it = filters.begin(); it != filters.end(); ++it) {
        const auto f = it.value().toObject();
        const auto type = f.value("type").toString();
        const auto label = f.value("label").toString(it.key());
        const auto options = f.value("options").toArray();
        QJsonArray labels;
        for (const auto &option : options)
            labels.append(option.toObject().value("label").toString());
        const auto valueAt = [&](int i) { return options.at(i).toObject().value("value"); };
        if (type == QLatin1String("Picker")) {
            int state = 0;
            for (int i = 0; i < options.size(); ++i)
                if (valueAt(i) == f.value("value"))
                    state = i;
            out.append(QJsonObject{{"key", it.key()}, {"type", "select"}, {"name", label}, {"values", labels}, {"state", state}});
        } else if (type == QLatin1String("Text")) {
            out.append(QJsonObject{{"key", it.key()}, {"type", "text"}, {"name", label}, {"state", f.value("value").toString()}});
        } else if (type == QLatin1String("Switch")) {
            out.append(QJsonObject{{"key", it.key()}, {"type", "checkbox"}, {"name", label}, {"state", f.value("value").toBool()}});
        } else if (type == QLatin1String("Checkbox") || type == QLatin1String("XCheckbox")) {
            const bool excludable = type == QLatin1String("XCheckbox");
            const auto value = f.value("value");
            QJsonArray children;
            for (int i = 0; i < options.size(); ++i) {
                const auto v = valueAt(i);
                if (excludable) {
                    const auto o = value.toObject();
                    const int state = o.value("include").toArray().contains(v) ? 1 : o.value("exclude").toArray().contains(v) ? 2 : 0;
                    children.append(QJsonObject{{"type", "tristate"}, {"name", labels.at(i)}, {"state", state}});
                } else {
                    children.append(QJsonObject{{"type", "checkbox"}, {"name", labels.at(i)}, {"state", value.toArray().contains(v)}});
                }
            }
            out.append(QJsonObject{{"key", it.key()}, {"type", "group"}, {"name", label}, {"filters", children}});
        }
    }
    return out;
}

QJsonObject PluginSource::fromFilterSheet(const QJsonObject &filters, const QJsonArray &states) {
    QJsonObject out = filters;
    for (const auto &value : states) {
        const auto s = value.toObject();
        const auto key = s.value("key").toString();
        if (!filters.contains(key))
            continue;
        auto f = filters.value(key).toObject();
        const auto options = f.value("options").toArray();
        const auto type = f.value("type").toString();
        if (type == QLatin1String("Picker")) {
            const int i = s.value("state").toInt();
            if (i >= 0 && i < options.size())
                f.insert(QStringLiteral("value"), options.at(i).toObject().value("value"));
        } else if (type == QLatin1String("Text")) {
            f.insert(QStringLiteral("value"), s.value("state").toString());
        } else if (type == QLatin1String("Switch")) {
            f.insert(QStringLiteral("value"), s.value("state").toBool());
        } else if (type == QLatin1String("Checkbox") || type == QLatin1String("XCheckbox")) {
            const auto children = s.value("filters").toArray();
            QJsonArray picked, include, exclude;
            for (int i = 0; i < std::min(children.size(), options.size()); ++i) {
                const auto v = options.at(i).toObject().value("value");
                const auto state = children.at(i).toObject().value("state");
                if (type == QLatin1String("Checkbox") && state.toBool())
                    picked.append(v);
                else if (state.toInt() == 1)
                    include.append(v);
                else if (state.toInt() == 2)
                    exclude.append(v);
            }
            f.insert(QStringLiteral("value"), type == QLatin1String("Checkbox") ? QJsonValue(picked)
                                                                                : QJsonValue(QJsonObject{{"include", include}, {"exclude", exclude}}));
        }
        out.insert(key, f);
    }
    return out;
}

// ---------------------------------------------------------------- settings
// LNReader keeps a plugin's settings in its storage, under their keys.
QJsonArray PluginSource::preferences() {
    const auto settings = Runtime::info(m_plugin).value("pluginSettings").toObject();
    QJsonArray out;
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        const auto s = it.value().toObject();
        const auto stored = Database::scalar(QStringLiteral("SELECT value FROM ext_prefs WHERE source_key = ? AND key = ?"),
                                             {key(), QStringLiteral("store:") + it.key()})
                                .toString();
        const auto value = stored.isEmpty() ? s.value("value") : QJsonDocument::fromJson(stored.toUtf8()).object().value("value");
        const auto type = s.value("type").toString();
        QJsonArray values, entries;
        for (const auto &option : s.value("options").toArray()) {
            values.append(option.toObject().value("value"));
            entries.append(option.toObject().value("label"));
        }
        QJsonObject p{{"key", it.key()}, {"title", s.value("label").toString(it.key())}, {"value", value}};
        if (type == QLatin1String("Switch"))
            p.insert(QStringLiteral("type"), QStringLiteral("switch"));
        else if (type == QLatin1String("Select"))
            p.insert(QStringLiteral("type"), QStringLiteral("list"));
        else if (type == QLatin1String("CheckboxGroup"))
            p.insert(QStringLiteral("type"), QStringLiteral("multiselect"));
        else
            p.insert(QStringLiteral("type"), QStringLiteral("text"));
        if (!values.isEmpty()) {
            p.insert(QStringLiteral("values"), values);
            p.insert(QStringLiteral("entries"), entries);
        }
        out.append(p);
    }
    return out;
}

void PluginSource::setPreference(const QString &setting, const QJsonValue &value) {
    Database::exec(QStringLiteral("INSERT INTO ext_prefs(source_key, key, value) VALUES(?, ?, ?) "
                                  "ON CONFLICT(source_key, key) DO UPDATE SET value = excluded.value"),
                   {key(), QStringLiteral("store:") + setting,
                    QString::fromUtf8(QJsonDocument(QJsonObject{{"value", value}}).toJson(QJsonDocument::Compact))});
    // The plugin reads its settings when it starts.
    Runtime::unload(key());
}

} // namespace sc::plugins
