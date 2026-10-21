// LNReader plugins in SCANCORE's runtime (src/plugins), offline: a fixture
// plugin (tests/fixtures/plugins) against a fixture site on novels.test.

#include "core/Cookies.h"
#include "core/Database.h"
#include "core/Http.h"
#include "plugins/Runtime.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace sc;
using namespace sc::plugins;

class Plugins : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    PluginRef m_plugin{QStringLiteral("lnreader:fixture"), QStringLiteral("fixture"),
                       QStringLiteral(SCANCORE_SOURCE_DIR "/tests/fixtures/plugins/fixture.js")};
    QByteArray m_lastSearch;

private slots:
    void initTestCase() {
        Database::open(m_dir.filePath(QStringLiteral("test.db")));
        Http::setTestHandler([this](const Request &request) -> std::optional<Response> {
            if (request.url.host() != QLatin1String("novels.test"))
                return std::nullopt;
            Response r;
            r.status = 200;
            r.finalUrl = request.url;
            r.contentType = "text/html; charset=utf-8";
            const auto path = request.url.path();
            if (path == QLatin1String("/popular")) {
                r.body = "<a href=\"/novel/1\" data-cover=\"https://novels.test/c1.jpg\">First Book</a>"
                         "<a href=\"/novel/2\" data-cover=\"\">Second Book</a>";
                if (request.url.query().contains(QLatin1String("sort=hot")))
                    r.body = "<a href=\"/novel/2\" data-cover=\"\">Second Book</a>";
            } else if (path == QLatin1String("/search") && request.method == "POST") {
                m_lastSearch = request.body;
                r.contentType = "application/json";
                r.body = R"({"results":[{"name":"First Book","path":"/novel/1"}]})";
            } else if (path == QLatin1String("/novel/1")) {
                r.body = "<h1>First Book</h1><div class=\"summary\">\n  A long\n  story.\n</div>"
                         "<li data-path=\"/novel/1/c1\" data-time=\"2026-01-01\">Chapter 1</li>"
                         "<li data-path=\"/novel/1/c2\" data-time=\"\">Chapter 2</li>";
            } else if (path == QLatin1String("/novel/1/c1")) {
                r.body = "<p>It began.</p><p>It went on.</p>";
            } else {
                r.status = 404;
            }
            return r;
        });
    }

    void declaresItself() {
        const auto info = Runtime::info(m_plugin);
        QCOMPARE(info.value("id").toString(), QStringLiteral("fixture"));
        QCOMPARE(info.value("version").toString(), QStringLiteral("1.2.0"));
        QCOMPARE(info.value("filters").toObject().value("sort").toObject().value("type").toString(), QStringLiteral("Picker"));
        QVERIFY(info.value("pluginSettings").toObject().contains("greeting"));
    }

    void browsesAndFilters() {
        QElapsedTimer timer;
        timer.start();
        const auto popular = Runtime::call(m_plugin, QStringLiteral("popularNovels"), {1, QJsonObject{{"showLatestNovels", false}}}).toArray();
        QCOMPARE(popular.size(), 2);
        QCOMPARE(popular[0].toObject().value("name").toString(), QStringLiteral("First Book"));
        QCOMPARE(popular[0].toObject().value("cover").toString(), QStringLiteral("https://novels.test/c1.jpg"));
        // A warm engine answers quickly (timer, fake network, regexes).
        timer.restart();
        const auto hot = Runtime::call(m_plugin, QStringLiteral("popularNovels"),
                                       {1, QJsonObject{{"filters", QJsonObject{{"sort", QJsonObject{{"value", "hot"}}}}}}})
                             .toArray();
        QCOMPARE(hot.size(), 1);
        QVERIFY2(timer.elapsed() < 200, qPrintable(QStringLiteral("%1 ms").arg(timer.elapsed())));
    }

    void searchesWithAForm() {
        const auto found = Runtime::call(m_plugin, QStringLiteral("searchNovels"), {QStringLiteral("first one"), 2}).toArray();
        QCOMPARE(found.size(), 1);
        QVERIFY(m_lastSearch.contains("name=\"q\"\r\n\r\nfirst one"));
        QVERIFY(m_lastSearch.contains("name=\"page\"\r\n\r\n2"));
    }

    void readsABookAndAChapter() {
        const auto novel = Runtime::call(m_plugin, QStringLiteral("parseNovel"), {QStringLiteral("/novel/1")}).toObject();
        QCOMPARE(novel.value("name").toString(), QStringLiteral("First Book"));
        // The "s" regex flag worked (the summary spans lines).
        QCOMPARE(novel.value("summary").toString(), QStringLiteral("A long\n  story."));
        QCOMPARE(novel.value("chapters").toArray().size(), 2);
        QCOMPARE(novel.value("status").toString(), QStringLiteral("Ongoing"));
        const auto html = Runtime::call(m_plugin, QStringLiteral("parseChapter"), {QStringLiteral("/novel/1/c1")}).toString();
        QCOMPARE(html, QStringLiteral("<p>It began.</p><p>It went on.</p>"));
    }

    void keepsItsStorage() {
        QCOMPARE(Runtime::call(m_plugin, QStringLiteral("remember"), {QStringLiteral("kept")}).toString(), QStringLiteral("kept"));
        // A fresh engine reads it back from the database.
        Runtime::unload(m_plugin.key);
        QCOMPARE(Runtime::call(m_plugin, QStringLiteral("recall")).toObject().value("value").toString(), QStringLiteral("kept"));
    }

    void bundledLibraries() {
        if (!Runtime::missingLibraries().isEmpty())
            QSKIP("This build has no bundled plugin libraries.");
        const auto html = QStringLiteral("<ul class=\"list\"><li><a href=\"/b/1\"> One </a></li><li class=\"ad\">Ad</li>"
                                         "<li><a href=\"/b/2\">Two</a></li></ul><p class=\"body\">Hi <b>there</b></p>");
        QElapsedTimer timer;
        timer.start();
        QJsonObject out;
        try {
            out = Runtime::call(m_plugin, QStringLiteral("scrape"), {html}).toObject();
        } catch (const Error &e) {
            QFAIL(qPrintable(e.message()));
        }
        QCOMPARE(out.value("titles").toArray(), (QJsonArray{"One", "Two"}));
        QCOMPARE(out.value("first").toString(), QStringLiteral("/b/1"));
        QCOMPARE(out.value("items").toInt(), 3);
        QCOMPARE(out.value("date").toString(), QStringLiteral("2026-01-13"));
        QCOMPARE(out.value("body").toString(), QStringLiteral("Hi <b>there</b>"));
        qInfo("cheerio, htmlparser2 and dayjs: %lld ms", timer.elapsed());
    }

    void webApis() {
        const auto out = Runtime::call(m_plugin, QStringLiteral("text")).toArray();
        QCOMPARE(out[0].toInt(), 6);
        QCOMPARE(out[1].toString(), QStringLiteral("héllo"));
        QCOMPARE(out[2].toString(), QStringLiteral("aGk="));
        QCOMPARE(out[3].toString(), QStringLiteral("hi"));
        QCOMPARE(out[4].toString(), QStringLiteral("https://x.test/a?b=1#c"));
    }

    void failuresAndLimits() {
        try {
            Runtime::call(m_plugin, QStringLiteral("fails"));
            QFAIL("no error");
        } catch (const Error &e) {
            QCOMPARE(e.message(), QStringLiteral("The site changed."));
        }
        // Plugins cannot reach this device or its network.
        try {
            Runtime::call(m_plugin, QStringLiteral("local"));
            QFAIL("reached a local address");
        } catch (const Error &e) {
            QVERIFY(e.message().contains(QLatin1String("this device")));
        }
        // Nor SCANCORE's own site or the sync server, whose cookies are the
        // account's session; the same hosts get no cookies from browser
        // checks or extensions.
        QVERIFY(CookieJar::isProtected(QUrl(QStringLiteral("https://scancore.onl/api/auth/session"))));
        QVERIFY(CookieJar::isProtected(QUrl(QStringLiteral("https://www.scancore.onl/"))));
        QVERIFY(!CookieJar::isProtected(QUrl(QStringLiteral("https://notscancore.onl/"))));
        QVERIFY(!CookieJar::isProtected(QUrl(QStringLiteral("https://sync.example.com/"))));
        Database::setPref(QStringLiteral("account.site"), QStringLiteral("https://sync.example.com"));
        CookieJar::refreshProtected();
        QVERIFY(CookieJar::isProtected(QUrl(QStringLiteral("https://sync.example.com/api/sync"))));
        for (const auto *url : {"https://scancore.onl/api/auth/session", "https://sync.example.com/api/sync"}) {
            try {
                Runtime::call(m_plugin, QStringLiteral("reach"), {QString::fromLatin1(url)});
                QFAIL("reached SCANCORE's own site");
            } catch (const Error &e) {
                QVERIFY(e.message().contains(QLatin1String("own site")));
            }
        }
        Database::setPref(QStringLiteral("account.site"), QString());
        CookieJar::refreshProtected();
        // A plugin that never returns is stopped at its limit, and the
        // engine still works afterwards.
        QElapsedTimer timer;
        timer.start();
        try {
            Runtime::call(m_plugin, QStringLiteral("spin"), {}, 800);
            QFAIL("spin returned");
        } catch (const Error &e) {
            QCOMPARE(e.status(), 504);
        }
        QVERIFY(timer.elapsed() < 2000);
        QCOMPARE(Runtime::call(m_plugin, QStringLiteral("popularNovels"), {1, QJsonObject{}}).toArray().size(), 2);
    }
};

QTEST_MAIN(Plugins)
#include "tst_plugins.moc"
