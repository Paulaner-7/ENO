#include "Downloads.h"

#include "Novels.h"
#include "core/Database.h"
#include "core/Http.h"
#include "sources/comics/Registry.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QStandardPaths>

namespace sc {

namespace {

constexpr int kConcurrent = 2;

QString hashed(const QString &value) {
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

QString defaultRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/downloads");
}

QString root() {
    const QString custom = Database::pref(QStringLiteral("setting.downloadsPath")).toString().trimmed();
    if (!custom.isEmpty())
        return custom;
    return defaultRoot();
}

QString extensionFor(const QByteArray &contentType, const QString &url) {
    if (contentType.contains("webp"))
        return QStringLiteral("webp");
    if (contentType.contains("png"))
        return QStringLiteral("png");
    if (contentType.contains("gif"))
        return QStringLiteral("gif");
    if (contentType.contains("avif"))
        return QStringLiteral("avif");
    if (contentType.contains("jpeg") || contentType.contains("jpg"))
        return QStringLiteral("jpg");
    const auto suffix = QFileInfo(QUrl(url).path()).suffix().toLower();
    return suffix.isEmpty() ? QStringLiteral("jpg") : suffix;
}

void writeJson(const QString &path, const QJsonDocument &doc) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        throw Error(QStringLiteral("Could not write to the downloads folder."));
    file.write(doc.toJson(QJsonDocument::Compact));
    if (!file.commit())
        throw Error(QStringLiteral("Could not write to the downloads folder."));
}

} // namespace

Downloads::Downloads(QObject *parent) : QObject(parent) {
    m_throttle.setSingleShot(true);
    m_throttle.setInterval(250);
    connect(&m_throttle, &QTimer::timeout, this, [this] {
        ++m_revision;
        emit changed();
    });
    // Anything interrupted by the last exit starts again.
    QTimer::singleShot(1500, this, [this] {
        Database::exec(QStringLiteral("UPDATE downloads SET status = 'queued' WHERE status = 'running'"));
        pump();
    });
}

Downloads *Downloads::instance() {
    static auto *downloads = [] {
        auto *created = new Downloads;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return downloads;
}

void Downloads::notify() {
    if (!m_throttle.isActive())
        m_throttle.start();
}

int Downloads::pending() const {
    return Database::scalar(QStringLiteral("SELECT COUNT(*) FROM downloads WHERE status IN ('queued', 'running')")).toInt();
}

QString Downloads::unitFolder(const QString &seriesId, const QString &unitKey) {
    const auto sub = QStringLiteral("%1/%2").arg(hashed(seriesId), hashed(unitKey));
    const auto custom = Database::pref(QStringLiteral("setting.downloadsPath")).toString().trimmed();
    if (!custom.isEmpty()) {
        const auto customPath = QStringLiteral("%1/%2").arg(custom, sub);
        if (QFile::exists(customPath + QStringLiteral("/index.json")) ||
            QFile::exists(customPath + QStringLiteral("/chapter.json"))) {
            return customPath;
        }
    }
    const auto def = defaultRoot();
    const auto defaultPath = QStringLiteral("%1/%2").arg(def, sub);
    if (!custom.isEmpty() && (QFile::exists(defaultPath + QStringLiteral("/index.json")) ||
                              QFile::exists(defaultPath + QStringLiteral("/chapter.json")))) {
        return defaultPath;
    }
    return QStringLiteral("%1/%2").arg(root(), sub);
}

void Downloads::enqueue(const QVariantMap &series, const QVariantList &units) {
    const auto seriesId = series.value(QStringLiteral("id")).toString();
    if (seriesId.isEmpty())
        return;
    Database::transaction([&] {
        for (const auto &value : units) {
            const auto u = value.toMap();
            Database::query(QStringLiteral(R"(
                INSERT INTO downloads(series_id, unit_key, unit_title, unit_url, status, created_at)
                VALUES(?, ?, ?, ?, 'queued', ?)
                ON CONFLICT(series_id, unit_key) DO UPDATE SET
                    status = CASE WHEN downloads.status = 'done' THEN 'done' ELSE 'queued' END, error = NULL
            )"),
                            {seriesId, u.value(QStringLiteral("key")).toString(), u.value(QStringLiteral("title")).toString(),
                             u.value(QStringLiteral("url")).toString(), Database::now()});
        }
    });
    notify();
    pump();
}

void Downloads::pump() {
    while (m_running.size() < kConcurrent) {
        QVariantList exclude;
        QStringList marks;
        for (int id : std::as_const(m_running)) {
            exclude.append(id);
            marks.append(QStringLiteral("?"));
        }
        const auto sql = QStringLiteral("SELECT d.*, s.title AS series_title FROM downloads d "
                                        "LEFT JOIN series s ON s.id = d.series_id WHERE d.status = 'queued'%1 "
                                        "ORDER BY d.created_at, d.id LIMIT 1")
                             .arg(marks.isEmpty() ? QString() : QStringLiteral(" AND d.id NOT IN (%1)").arg(marks.join(',')));
        const auto row = Database::one(sql, exclude);
        if (row.isEmpty())
            return;
        const int id = row.value(QStringLiteral("id")).toInt();
        m_running.insert(id);
        Database::exec(QStringLiteral("UPDATE downloads SET status = 'running' WHERE id = ?"), {id});
        notify();
        runInBackground([this, row] { runJob(row); });
    }
}

void Downloads::runJob(const QVariantMap &row) {
    const int id = row.value(QStringLiteral("id")).toInt();
    const auto seriesId = row.value(QStringLiteral("series_id")).toString();
    const auto unitKey = row.value(QStringLiteral("unit_key")).toString();
    const auto unitUrl = row.value(QStringLiteral("unit_url")).toString();
    const auto folder = unitFolder(seriesId, unitKey);
    QString error;
    try {
        QDir().mkpath(folder);
        if (seriesId.startsWith(QLatin1String("novel:"))) {
            const auto chapter = Novels::fetchChapter(unitUrl);
            writeJson(folder + QStringLiteral("/chapter.json"), QJsonDocument::fromVariant(chapter));
            Database::exec(QStringLiteral("UPDATE downloads SET done = 1, total = 1 WHERE id = ?"), {id});
        } else if (seriesId.startsWith(QLatin1String("comic:"))) {
            auto source = comics::Registry::forUrl(unitUrl);
            if (!source)
                throw Error(QStringLiteral("This source is no longer available."));
            const auto panels = source->panels(unitUrl);
            if (panels.isEmpty())
                throw Error(QStringLiteral("This chapter has no readable pages."));
            Database::exec(QStringLiteral("UPDATE downloads SET total = ?, done = 0 WHERE id = ?"), {panels.size(), id});
            QJsonArray index;
            for (int i = 0; i < panels.size(); ++i) {
                if (CancelScope::cancelled())
                    throw Error(QStringLiteral("Cancelled"), 499);
                Request request;
                request.url = QUrl(panels[i].url);
                request.lane = Lane::Images;
                request.timeoutMs = 30000;
                request.maxBytes = 40 * 1024 * 1024;
                request.headers = {{"Referer", source->imageReferer().toUtf8()},
                                   {"Accept", "image/avif,image/webp,image/*,*/*;q=0.8"}};
                Response response;
                for (int attempt = 0; attempt < 3; ++attempt) {
                    try {
                        response = Http::send(request);
                        if (response.ok())
                            break;
                    } catch (const Error &e) {
                        if (attempt == 2 || e.status() == 499)
                            throw;
                    }
                }
                if (!response.ok())
                    throw Error(QStringLiteral("Page %1 could not be downloaded (HTTP %2).").arg(i + 1).arg(response.status));
                const auto name = QStringLiteral("%1.%2").arg(i + 1, 4, 10, QLatin1Char('0'))
                                      .arg(extensionFor(response.contentType, panels[i].url));
                QSaveFile file(folder + QLatin1Char('/') + name);
                if (!file.open(QIODevice::WriteOnly) || file.write(response.body) != response.body.size() || !file.commit())
                    throw Error(QStringLiteral("Could not write to the downloads folder."));
                index.append(QJsonObject{{"file", name}, {"width", panels[i].width}, {"height", panels[i].height}});
                Database::exec(QStringLiteral("UPDATE downloads SET done = ? WHERE id = ?"), {i + 1, id});
                QMetaObject::invokeMethod(this, [this] { notify(); });
            }
            writeJson(folder + QStringLiteral("/index.json"), QJsonDocument(QJsonObject{{"panels", index}}));
        } else {
            throw Error(QStringLiteral("Only comics and novels can be downloaded."));
        }
    } catch (const Error &e) {
        error = e.message();
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
    }
    Database::exec(QStringLiteral("UPDATE downloads SET status = ?, error = ?, path = ? WHERE id = ?"),
                   {error.isEmpty() ? QStringLiteral("done") : QStringLiteral("failed"),
                    error.isEmpty() ? QVariant() : QVariant(error), folder, id});
    const auto title = QStringLiteral("%1 · %2").arg(row.value(QStringLiteral("series_title")).toString(),
                                                      row.value(QStringLiteral("unit_title")).toString());
    QMetaObject::invokeMethod(this, [this, id, title, ok = error.isEmpty()] {
        m_running.remove(id);
        notify();
        if (ok)
            emit finished(title);
        pump();
    });
}

Task *Downloads::items() {
    return Task::run([]() -> QVariant {
        return Database::query(QStringLiteral(
            "SELECT d.id, d.series_id AS seriesId, d.unit_key AS unitKey, d.unit_title AS unitTitle, d.unit_url AS unitUrl, "
            "d.status, d.done, d.total, d.error, d.created_at AS createdAt, s.title AS seriesTitle, s.cover, s.media, "
            "s.source_url AS sourceUrl, s.source FROM downloads d LEFT JOIN series s ON s.id = d.series_id "
            "ORDER BY CASE d.status WHEN 'running' THEN 0 WHEN 'queued' THEN 1 WHEN 'failed' THEN 2 ELSE 3 END, "
            "d.created_at DESC, d.id"));
    });
}

QVariantMap Downloads::states(const QString &seriesId) const {
    QVariantMap out;
    for (const auto &value : Database::query(
             QStringLiteral("SELECT unit_key, status, done, total FROM downloads WHERE series_id = ?"), {seriesId})) {
        const auto row = value.toMap();
        out.insert(row.value(QStringLiteral("unit_key")).toString(),
                   QVariantMap{{"status", row.value(QStringLiteral("status"))},
                               {"done", row.value(QStringLiteral("done"))},
                               {"total", row.value(QStringLiteral("total"))}});
    }
    return out;
}

void Downloads::remove(int id) {
    const auto row = Database::one(QStringLiteral("SELECT series_id, unit_key FROM downloads WHERE id = ?"), {id});
    if (row.isEmpty() || m_running.contains(id))
        return;
    QDir(unitFolder(row.value(QStringLiteral("series_id")).toString(), row.value(QStringLiteral("unit_key")).toString()))
        .removeRecursively();
    Database::exec(QStringLiteral("DELETE FROM downloads WHERE id = ?"), {id});
    notify();
}

void Downloads::removeUnit(const QString &seriesId, const QString &unitKey) {
    const auto id = Database::scalar(QStringLiteral("SELECT id FROM downloads WHERE series_id = ? AND unit_key = ?"),
                                     {seriesId, unitKey});
    if (id.isValid())
        remove(id.toInt());
}

void Downloads::retry(int id) {
    Database::exec(QStringLiteral("UPDATE downloads SET status = 'queued', error = NULL WHERE id = ? AND status = 'failed'"),
                   {id});
    notify();
    pump();
}

void Downloads::retryFailed() {
    Database::exec(QStringLiteral("UPDATE downloads SET status = 'queued', error = NULL WHERE status = 'failed'"));
    notify();
    pump();
}

void Downloads::clearFinished() {
    Database::exec(QStringLiteral("DELETE FROM downloads WHERE status = 'failed'"));
    notify();
}

QString Downloads::folder() const { return root(); }

bool Downloads::hasCustomFolder() const {
    return !Database::pref(QStringLiteral("setting.downloadsPath")).toString().trimmed().isEmpty();
}

void Downloads::setFolder(const QString &folder) {
    QString path = folder;
    if (path.startsWith(QLatin1String("file://")))
        path = QUrl(folder).toLocalFile();
    path = path.trimmed();
    if (path.isEmpty())
        return;
    QDir().mkpath(path);
    Database::setPref(QStringLiteral("setting.downloadsPath"), path);
    notify();
}

void Downloads::resetFolder() {
    Database::setPref(QStringLiteral("setting.downloadsPath"), QString());
    notify();
}

QString Downloads::sizeOnDisk() const {
    qint64 total = 0;
    QDirIterator it(root(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    if (hasCustomFolder()) {
        QDirIterator itDef(defaultRoot(), QDir::Files, QDirIterator::Subdirectories);
        while (itDef.hasNext()) {
            itDef.next();
            total += itDef.fileInfo().size();
        }
    }
    return QLocale().formattedDataSize(total);
}

QVariantList Downloads::localPanels(const QString &seriesId, const QString &unitKey) {
    const auto folder = unitFolder(seriesId, unitKey);
    QFile file(folder + QStringLiteral("/index.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QVariantList out;
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).object().value("panels").toArray()) {
        const auto p = value.toObject();
        out.append(QVariantMap{{"url", QUrl::fromLocalFile(folder + QLatin1Char('/') + p.value("file").toString()).toString()},
                               {"width", p.value("width").toInt()},
                               {"height", p.value("height").toInt()}});
    }
    return out;
}

QVariantMap Downloads::localNovelChapter(const QString &seriesId, const QString &unitKey) {
    QFile file(unitFolder(seriesId, unitKey) + QStringLiteral("/chapter.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
}

} // namespace sc
