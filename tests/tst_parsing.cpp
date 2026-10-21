// Offline tests for the HTML layer and parsers. Live site checks are a
// separate, opt-in run because sites change and CI must stay deterministic.

#include "core/Html.h"
#include "core/Text.h"
#include "sources/Manifest.h"
#include "sources/comics/ComicSource.h"
#include "sources/comics/Engines.h"
#include "sources/comics/Registry.h"
#include "sources/local/Local.h"
#include "sources/novels/NovelSource.h"
#include "sources/novels/Registry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QtTest>

using namespace sc;

// Markup and data shaped like each site's, in tests/fixtures.
static QByteArray fixture(const char *name) {
    QFile file(QStringLiteral(SCANCORE_SOURCE_DIR "/tests/fixtures/") + QLatin1String(name));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

class Parsing : public QObject {
    Q_OBJECT

private slots:
    void selectorsReturnDocumentOrder() {
        HtmlDocument doc(QStringLiteral("<ul><li class=a>1</li><li class=b>2</li><li class=a>3</li></ul>"));
        const auto nodes = doc.select(QStringLiteral("li.a, li.b"));
        QCOMPARE(nodes.size(), 3);
        QCOMPARE(nodes[0].text(), QStringLiteral("1"));
        QCOMPARE(nodes[2].text(), QStringLiteral("3"));
    }

    void attributesAndClosest() {
        HtmlDocument doc(QStringLiteral("<div class=\"row\"><h3 class=\"title\"><a href=\"/n/x\" title=\"X\">X</a></h3><img data-src=\"/c.jpg\"></div>"));
        const auto a = doc.first(QStringLiteral("h3[class*='title'] > a"));
        QCOMPARE(a.attr("title"), QStringLiteral("X"));
        const auto row = a.closest(QStringLiteral(".row"));
        QVERIFY(!row.isNull());
        QCOMPARE(novels::imgSrc(row.first(QStringLiteral("img")), QStringLiteral("https://example.com/")),
                 QStringLiteral("https://example.com/c.jpg"));
    }

    void resolveRejectsScripts() {
        QVERIFY(resolveUrl(QStringLiteral("https://a.test/"), QStringLiteral("javascript:alert(1)")).isEmpty());
        QVERIFY(resolveUrl(QStringLiteral("https://a.test/"), QStringLiteral("data:image/png;base64,AA")).isEmpty());
        QCOMPARE(resolveUrl(QStringLiteral("https://a.test/x/"), QStringLiteral("../y")), QStringLiteral("https://a.test/y"));
    }

    void entities() {
        QCOMPARE(decodeEntities(QStringLiteral("Tom &amp; Jerry &#8211; Ep")), QString::fromUtf8("Tom & Jerry – Ep"));
    }

    void textBlocksKeepFormattingOnly() {
        HtmlDocument doc(QStringLiteral(
            "<div id=c><p>Hello <i>world</i> <script>x()</script></p><p style=\"display:none\">hidden</p>"
            "<h2>Title</h2>line one<br>line two<p><a href=\"https://evil.test\" onclick=\"x\">link</a> &lt;b&gt;</p></div>"));
        const auto blocks = novels::textBlocks(doc.first(QStringLiteral("#c")));
        QCOMPARE(blocks.size(), 5);
        QCOMPARE(blocks[0].text, QStringLiteral("Hello world"));
        QCOMPARE(blocks[0].html, QStringLiteral("Hello <i>world</i>"));
        QCOMPARE(blocks[1].kind, QStringLiteral("heading"));
        QCOMPARE(blocks[2].text, QStringLiteral("line one"));
        QCOMPARE(blocks[3].text, QStringLiteral("line two"));
        // Markup from the page arrives escaped, never as tags.
        QCOMPARE(blocks[4].html, QStringLiteral("link &lt;b&gt;"));
    }

    void chapterNumbers() {
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Chapter 12.5 - The end")), QStringLiteral("12.5"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Ep. 7")), QStringLiteral("7"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Prologue")), QStringLiteral("0"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Season 2 Chapter 14")), QStringLiteral("14"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Ch.5")), QStringLiteral("5"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Vol. 3 Chap 21")), QStringLiteral("21"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Episode 40")), QStringLiteral("40"));
        // Spanish labels.
        QCOMPARE(comics::chapterNumberFrom(QString::fromUtf8("Capítulo 12")), QStringLiteral("12"));
        QCOMPARE(comics::chapterNumberFrom(QString::fromUtf8("CAPÍTULO 7.5")), QStringLiteral("7.5"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Capitulo 3")), QStringLiteral("3"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Cap. 12")), QStringLiteral("12"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Cap 9: El regreso")), QStringLiteral("9"));
        QCOMPARE(comics::chapterNumberFrom(QString::fromUtf8("Temporada 2 Capítulo 12")), QStringLiteral("12"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Episodio 4")), QStringLiteral("4"));
        // Decomposed accents (NFD) read the same.
        QCOMPARE(comics::chapterNumberFrom(QString::fromUtf8("Capi\xcc\x81tulo 30")), QStringLiteral("30"));
        // A label has to start a word: "Batch" and "Match" are not "ch".
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Match 2 Chapter 10")), QStringLiteral("10"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Captain 3")), QStringLiteral("3"));
        QList<comics::Chapter> list{{QStringLiteral("10")}, {QStringLiteral("2")}, {QStringLiteral("2.5")}};
        comics::sortChapters(list);
        QCOMPARE(list[0].number, QStringLiteral("2"));
        QCOMPARE(list[2].number, QStringLiteral("10"));
    }

    void titleNormalizer() {
        // Plain English titles: as before, words split on anything else.
        QCOMPARE(normalizedTitle(QStringLiteral("Solo Leveling: Ragnarok")), QStringLiteral("solo leveling ragnarok"));
        QCOMPARE(normalizedTitle(QStringLiteral("  The  Beginning-After_the END! ")), QStringLiteral("the beginning after the end"));
        QCOMPARE(normalizedTitle(QStringLiteral("Re:Zero (2nd Season)")), QStringLiteral("re zero 2nd season"));
        QCOMPARE(titleKey(QStringLiteral("Re:Zero (2nd Season)")), QStringLiteral("rezero2ndseason"));
        // Accents and the tilde fold away; Spanish punctuation separates.
        QCOMPARE(normalizedTitle(QString::fromUtf8("Año 2")), QStringLiteral("ano 2"));
        QCOMPARE(normalizedTitle(QString::fromUtf8("¿Quién es él?")), QStringLiteral("quien es el"));
        QCOMPARE(normalizedTitle(QString::fromUtf8("¡La Pequeña Señorita!")), QStringLiteral("la pequena senorita"));
        QCOMPARE(titleKey(QString::fromUtf8("Pequeña")), titleKey(QStringLiteral("pequena")));
        QCOMPARE(titleKey(QString::fromUtf8("PINGÜINO")), QStringLiteral("pinguino"));
        // Decomposed input folds the same as composed input.
        QCOMPARE(normalizedTitle(QString::fromUtf8("Pequen\xcc\x83" "a")), QStringLiteral("pequena"));
        // Letters with no accent to drop are kept, not split.
        QCOMPARE(normalizedTitle(QString::fromUtf8("Ørsted Æon")), QString::fromUtf8("ørsted æon"));
        // Other scripts stay whole and distinct.
        QCOMPARE(normalizedTitle(QString::fromUtf8("俺だけレベルアップな件")), QString::fromUtf8("俺だけレベルアップな件"));
        QCOMPARE(normalizedTitle(QString::fromUtf8("나 혼자만 레벨업")), QString::fromUtf8("나 혼자만 레벨업"));
        QCOMPARE(titleKey(QString::fromUtf8("全职法师 2")), QString::fromUtf8("全职法师2"));
        QVERIFY(!normalizedTitle(QString::fromUtf8("進撃の巨人")).isEmpty());
        // Japanese voicing marks change the word, so they are not dropped.
        QVERIFY(titleKey(QString::fromUtf8("がっこう")) != titleKey(QString::fromUtf8("かっこう")));
        QCOMPARE(normalizedTitle(QString::fromUtf8("Только я")), QString::fromUtf8("только я"));
        QVERIFY(normalizedTitle(QStringLiteral("?!... --")).isEmpty());
    }

    void languageNames() {
        QCOMPARE(languageName(QStringLiteral("en")), QStringLiteral("English"));
        QCOMPARE(languageName(QStringLiteral("es")), QString::fromUtf8("Español"));
        QCOMPARE(languageName(QStringLiteral("es-la")), QString::fromUtf8("Español"));
        QCOMPARE(languageName(QStringLiteral("zz")), QStringLiteral("zz"));
    }

    void chapterLabels() {
        QCOMPARE(comics::chapterLabel(QStringLiteral("en"), QStringLiteral("3")), QStringLiteral("Chapter 3"));
        QCOMPARE(comics::chapterLabel(QStringLiteral("en"), QStringLiteral("3"), QStringLiteral("Start")), QStringLiteral("Chapter 3: Start"));
        QCOMPARE(comics::chapterLabel(QStringLiteral("es"), QStringLiteral("3")), QString::fromUtf8("Capítulo 3"));
        QCOMPARE(comics::chapterLabel(QStringLiteral("es-la"), QStringLiteral("4.5"), QStringLiteral("Fin")), QString::fromUtf8("Capítulo 4.5: Fin"));
        QCOMPARE(comics::chapterLabel(QStringLiteral("it"), QStringLiteral("12")), QStringLiteral("Capitolo 12"));
        // Unknown languages fall back to English.
        QCOMPARE(comics::chapterLabel(QStringLiteral("xx"), QStringLiteral("1")), QStringLiteral("Chapter 1"));
    }

    void minVersion() {
        QVERIFY(!Manifest::appVersion().isEmpty());
        QVERIFY(Manifest::supports(QJsonObject{}));
        QVERIFY(Manifest::supports(QJsonObject{{"minVersion", "1.0.0"}}));
        QVERIFY(Manifest::supports(QJsonObject{{"minVersion", Manifest::appVersion()}}));
        QVERIFY(!Manifest::supports(QJsonObject{{"minVersion", "999.0"}}));
        // A version this build cannot read counts as newer.
        QVERIFY(!Manifest::supports(QJsonObject{{"minVersion", "soon"}}));
    }

    void registriesReadNewFields() {
        const auto manifest = QJsonDocument(QJsonObject{
            {"version", 1000000},
            {"comics", QJsonArray{
                QJsonObject{{"key", "dex"}, {"engine", "mangadex"}, {"base", "https://mangadex.org"}, {"hosts", QJsonArray{"mangadex.org"}}},
                QJsonObject{{"key", "dex-es"}, {"label", QString::fromUtf8("MangaDex (Español)")}, {"engine", "mangadex"},
                            {"base", "https://mangadex.org"}, {"hosts", QJsonArray{"mangadex.org"}}, {"language", "es"},
                            {"urlTag", "es"}, {"minVersion", "1.0.0"}, {"options", QJsonObject{{"languages", QJsonArray{"es", "es-la"}}}}},
                QJsonObject{{"key", "future"}, {"engine", "madara"}, {"base", "https://future.test"},
                            {"hosts", QJsonArray{"future.test"}}, {"minVersion", "999.0"}},
                QJsonObject{{"key", "vortex"}, {"engine", "iken"}, {"base", "https://vortexscans.org"}, {"hosts", QJsonArray{"vortexscans.org"}}},
                QJsonObject{{"key", "lector"}, {"engine", "madara"}, {"base", "https://lector.test"},
                            {"hosts", QJsonArray{"lector.test"}}, {"language", "es"}, {"nsfw", false}}}},
            {"novels", QJsonArray{
                QJsonObject{{"key", "novelas"}, {"engine", "novelfull"}, {"base", "https://novelas.test"},
                            {"hosts", QJsonArray{"novelas.test"}}, {"language", "es"}},
                QJsonObject{{"key", "later"}, {"engine", "novelfull"}, {"base", "https://later.test"},
                            {"hosts", QJsonArray{"later.test"}}, {"minVersion", "999.0"}}}}});
        QVERIFY(Manifest::loadData(manifest.toJson()));
        comics::Registry::rebuild();
        novels::Registry::rebuild();

        // Fields and defaults.
        const auto dex = comics::Registry::byKey(QStringLiteral("dex"));
        const auto dexEs = comics::Registry::byKey(QStringLiteral("dex-es"));
        QVERIFY(dex && dexEs);
        QCOMPARE(dex->language(), QStringLiteral("en"));
        QVERIFY(dex->config().urlTag.isEmpty());
        QCOMPARE(dexEs->language(), QStringLiteral("es"));
        QCOMPARE(dexEs->config().urlTag, QStringLiteral("es"));
        QCOMPARE(dexEs->config().options.value("languages").toArray().size(), 2);
        QCOMPARE(comics::Registry::byKey(QStringLiteral("lector"))->language(), QStringLiteral("es"));
        QCOMPARE(novels::Registry::byKey(QStringLiteral("novelas"))->config().language, QStringLiteral("es"));

        // Entries for a newer app are skipped.
        QVERIFY(!comics::Registry::byKey(QStringLiteral("future")));
        QVERIFY(!novels::Registry::byKey(QStringLiteral("later")));

        // Links: the tag picks the tagged source; untagged links (and every
        // id already in a library) stay with the untagged one.
        const auto keyFor = [](const QString &url) {
            const auto source = comics::Registry::forUrl(url);
            return source ? source->key() : QString();
        };
        QCOMPARE(keyFor(QStringLiteral("https://mangadex.org/title/0b1c-x")), QStringLiteral("dex"));
        QCOMPARE(keyFor(QStringLiteral("https://mangadex.org/chapter/9f-y")), QStringLiteral("dex"));
        QCOMPARE(keyFor(QStringLiteral("https://mangadex.org/title/0b1c-x#es")), QStringLiteral("dex-es"));
        QCOMPARE(keyFor(QStringLiteral("https://mangadex.org/chapter/9f-y#es")), QStringLiteral("dex-es"));
        // Iken keeps its numeric post id in the fragment.
        QCOMPARE(keyFor(QStringLiteral("https://vortexscans.org/series/some-title#1234")), QStringLiteral("vortex"));
        // A tag on a host its source does not own is nobody's.
        QCOMPARE(keyFor(QStringLiteral("https://vortexscans.org/series/some-title#es")), QString());
        QCOMPARE(keyFor(QStringLiteral("https://unknown.test/x")), QString());
        QCOMPARE(novels::Registry::forUrl(QStringLiteral("https://novelas.test/libro"))->key(), QStringLiteral("novelas"));
    }

    void manifestSmartMerge() {
        Manifest::load(QString());
        const auto builtIn = Manifest::current();
        QVERIFY(!builtIn.isEmpty());
        QVERIFY(builtIn.value("enoExclusive").toArray().contains(QStringLiteral("comics")));

        // An upstream refresh: new version, vanilla comic sources, and fixed
        // novel sources. ENO must keep only MangaWorld for comics.
        const auto upstream = QJsonDocument(QJsonObject{
            {"version", 999},
            {"comics", QJsonArray{QJsonObject{{"key", "asura"}, {"engine", "asura"},
                                              {"base", "https://asurascans.com"}, {"hosts", QJsonArray{"asurascans.com"}}}}},
            {"novels", QJsonArray{QJsonObject{{"key", "novelfull"}, {"engine", "novelfull"},
                                              {"base", "https://upstream.test"}, {"hosts", QJsonArray{"upstream.test"}}},
                                  QJsonObject{{"key", "novelas"}, {"engine", "novelfull"},
                                              {"base", "https://novelas.test"}, {"hosts", QJsonArray{"novelas.test"}}}}}})
                                .toJson();
        QVERIFY(Manifest::loadData(upstream));
        const auto applied = Manifest::current();
        QCOMPARE(applied.value("version").toInt(), 999);

        const auto comics = applied.value("comics").toArray();
        QCOMPARE(comics.size(), 1);
        QCOMPARE(comics[0].toObject().value("key").toString(), QStringLiteral("mangaworld"));

        // Novels: the upstream entry wins for a shared key, upstream-only
        // entries are added, and ENO-only sources are kept.
        QHash<QString, QString> bases;
        for (const auto &value : applied.value("novels").toArray()) {
            const auto o = value.toObject();
            bases.insert(o.value("key").toString(), o.value("base").toString());
        }
        QCOMPARE(bases.value(QStringLiteral("novelfull")), QStringLiteral("https://upstream.test"));
        QCOMPARE(bases.value(QStringLiteral("novelas")), QStringLiteral("https://novelas.test"));
        for (const auto &value : builtIn.value("novels").toArray())
            QVERIFY(bases.contains(value.toObject().value("key").toString()));
    }

    void challengeDetection() {
        Response page;
        page.status = 200;
        page.contentType = "text/html; charset=UTF-8";
        // Cloudflare's beacon on an ordinary page is not a check.
        page.body = "<html><head><title>Solo Leveling - M440</title>"
                    "<script src=\"/cdn-cgi/challenge-platform/scripts/jsd/main.js\"></script></head></html>";
        QVERIFY(!page.isChallenge());
        page.status = 403;
        QVERIFY(page.isChallenge());
        page.status = 200;
        page.body = "<!DOCTYPE html><html><head><title>Just a moment...</title></head></html>";
        QVERIFY(page.isChallenge());
    }

    void leerCapitulo() {
        const QString base = QStringLiteral("https://www.leercapitulo.co");
        const auto results = comics::LeerCapituloSource::parseSearch(fixture("leercapitulo-search.html"), base);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].title, QStringLiteral("Level Up Alone"));
        QCOMPARE(results[0].url, QStringLiteral("https://www.leercapitulo.co/manga/0wdem0o1jg/level-up-alone/"));
        QCOMPARE(results[0].cover, QStringLiteral("https://www.leercapitulo.co/covers/95/be.jpg?v1"));
        QCOMPARE(results[0].type, QStringLiteral("Manhwa"));

        const QString series = base + QStringLiteral("/manga/0rradr8n/fin/");
        const auto chapters = comics::LeerCapituloSource::parseChapters(fixture("leercapitulo-chapters.html"), series);
        QCOMPARE(chapters.size(), 3);
        QCOMPARE(chapters[0].number, QStringLiteral("1"));
        QCOMPARE(chapters[1].number, QStringLiteral("53.5"));
        QCOMPARE(chapters[2].url, QStringLiteral("https://www.leercapitulo.co/leer/0rradr8n/fin/54/"));
        QCOMPARE(chapters[2].released, QStringLiteral("2026-10-19"));

        const auto panels = comics::LeerCapituloSource::parsePanels(fixture("leercapitulo-pages.html"), base + QStringLiteral("/leer/x/y/1/"));
        QCOMPARE(panels.size(), 2);
        QCOMPARE(panels[1].url, QStringLiteral("https://img.test/a/2.webp"));
    }

    void mangaverse() {
        const QString base = QStringLiteral("https://www.mangaoni.com/es");
        const auto cards = comics::MangaverseSource::parseCards(fixture("mangaverse-search.html"), base, QStringLiteral("ES"));
        QCOMPARE(cards.size(), 1);
        QCOMPARE(cards[0].title, QStringLiteral("Solo Leveling"));
        QCOMPARE(cards[0].cover, QStringLiteral("https://www.mangaoni.com/wp-content/uploads/c.jpg"));
        QCOMPARE(comics::MangaverseSource::cleanTitle(QStringLiteral("Wind Breaker ES"), QStringLiteral("ES")), QStringLiteral("Wind Breaker"));
        QCOMPARE(comics::MangaverseSource::cleanTitle(QStringLiteral("Hades"), QStringLiteral("ES")), QStringLiteral("Hades"));

        const auto chapters = comics::MangaverseSource::parseChapters(fixture("mangaverse-chapters.html"), base + QStringLiteral("/manga/sl.html"));
        QCOMPARE(chapters.size(), 2);
        QCOMPARE(chapters[0].title, QStringLiteral("Capitulo 201"));
        QCOMPARE(chapters[0].number, QStringLiteral("201"));
        QCOMPARE(chapters[1].number, QStringLiteral("200.5"));
    }

    void inManga() {
        const auto results = comics::InMangaSource::parseSearch(fixture("inmanga-search.html"), QStringLiteral("https://inmanga.com"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].title, QStringLiteral("One Piece"));
        QCOMPARE(results[0].url, QStringLiteral("https://inmanga.com/ver/manga/One-Piece/dfc7ecb5-e9b3-4aa5-a61b-a498993cd935"));
        QCOMPARE(results[0].cover, QStringLiteral("https://cdn1.intomanga.com/i/m/dfc7ecb5/t/o/dfc7ecb5.jpg"));

        // The page picker appears twice; ids are lower-cased into the path.
        const QByteArray controls = fixture("inmanga-pages.html");
        const auto pages = comics::InMangaSource::parsePages(controls, QStringLiteral("https://cdn1.intomanga.com"),
                                                             QStringLiteral("DDAB755F-B5A2"));
        QCOMPARE(pages.size(), 2);
        QCOMPARE(pages[0].url, QStringLiteral("https://cdn1.intomanga.com/i/m/dfc7ecb5-aaaa/c/ddab755f-b5a2/o/32dfc965-0001.jpg"));
        QVERIFY(comics::InMangaSource::parsePages("<select id=\"PageList\"></select>", QStringLiteral("https://x"), QStringLiteral("y")).isEmpty());
    }

    void mangaWorld() {
        const QString base = QStringLiteral("https://www.mangaworld.mx");
        const auto cards = comics::MangaWorldSource::parseCards(fixture("mangaworld-search.html"), base);
        QCOMPARE(cards.size(), 1);
        QCOMPARE(cards[0].title, QStringLiteral("Fairy Tail"));
        QCOMPARE(cards[0].url, QStringLiteral("https://www.mangaworld.mx/manga/785/fairy-tail"));
        QCOMPARE(cards[0].cover, QStringLiteral("https://cdn.mangaworld.mx/mangas/5f79078355b2325c6be650bb.jpg?1"));
        QCOMPARE(cards[0].type, QStringLiteral("Manga"));
        QCOMPARE(cards[0].author, QStringLiteral("MASHIMA Hiro"));
        QCOMPARE(cards[0].genres, QStringLiteral("Azione, Avventura"));

        const auto chapters = comics::MangaWorldSource::parseChapters(fixture("mangaworld-chapters.html"),
                                                                      base + QStringLiteral("/manga/785/fairy-tail"));
        QCOMPARE(chapters.size(), 2);
        QCOMPARE(chapters[0].number, QStringLiteral("544"));
        QCOMPARE(chapters[1].number, QStringLiteral("545"));
        QCOMPARE(chapters[1].title, QStringLiteral("Capitolo 545"));
        QCOMPARE(chapters[1].slug, QStringLiteral("aaa"));
        QCOMPARE(chapters[1].released, QStringLiteral("2026-10-18"));

        const auto panels = comics::MangaWorldSource::parsePanels(fixture("mangaworld-pages.html"),
                                                                  base + QStringLiteral("/manga/785/fairy-tail/read/aaa"));
        QCOMPARE(panels.size(), 2);
        QCOMPARE(panels[1].url, QStringLiteral("https://cdn.mangaworld.mx/chapters/x/2.png"));
    }

    void manhwaWeb() {
        // Links can name an older id of the series; chapters use the current one.
        const auto series = QJsonDocument::fromJson(fixture("manhwaweb-series.json")).object();
        const auto chapters = comics::ManhwaWebSource::parseChapters(series, QStringLiteral("comic-solo_leveling_169"),
                                                                     QStringLiteral("https://manhwaweb.com"), QStringLiteral("es"));
        QCOMPARE(chapters.size(), 2);
        QCOMPARE(chapters[0].number, QStringLiteral("0.01"));
        QCOMPARE(chapters[1].url, QStringLiteral("https://manhwaweb.com/leer/comic-solo_leveling_169-1_01"));
        QCOMPARE(chapters[1].title, QString::fromUtf8("Capítulo 1"));
        QCOMPARE(chapters[1].released, QStringLiteral("2023-08-07T19:20:00Z"));
    }

    void gunzip() {
        // Python's gzip, with a file name in the header.
        const auto packed = QByteArray::fromHex("1f8b08080000000002ff6261636b75702e7461636869626b000b7676f473f60f7255484a4cce2e2d50081ee58ff2e9c8070008b786ca80020000");
        QCOMPARE(local::gunzip(packed), QByteArray("SCANCORE backup ").repeated(40));
        QVERIFY(local::gunzip(packed.left(12)).isEmpty());
        QVERIFY(local::gunzip("not gzip at all").isEmpty());
    }

    void manifestIsValid() {
        QFile file(QStringLiteral(SCANCORE_SOURCE_DIR "/sources/manifest.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(file.readAll()).object();
        QVERIFY(doc.value("version").toInt() >= 1);
        QSet<QString> keys;
        for (const auto &section : {QStringLiteral("comics"), QStringLiteral("novels")})
            for (const auto &value : doc.value(section).toArray()) {
                const auto o = value.toObject();
                const auto key = section + o.value("key").toString();
                QVERIFY2(!keys.contains(key), qPrintable(key));
                keys.insert(key);
                QVERIFY(o.value("base").toString().startsWith(QLatin1String("https://")));
                QVERIFY(!o.value("hosts").toArray().isEmpty());
                // Optional fields: a short language code, a tag, an app version.
                static const QRegularExpression code(QStringLiteral("^[a-z]{2,3}(-[a-z0-9]{2,8})?$"));
                if (o.contains("language"))
                    QVERIFY2(code.match(o.value("language").toString()).hasMatch(), qPrintable(key));
                if (o.contains("urlTag"))
                    QVERIFY2(!o.value("urlTag").toString().isEmpty(), qPrintable(key));
                if (o.contains("minVersion"))
                    QVERIFY2(!QVersionNumber::fromString(o.value("minVersion").toString()).isNull(), qPrintable(key));
                if (section == QLatin1String("comics")) {
                    comics::SourceConfig config;
                    config.engine = o.value("engine").toString();
                    QVERIFY2(comics::createSource(config) != nullptr, qPrintable(config.engine));
                } else {
                    novels::SourceConfig config;
                    config.engine = o.value("engine").toString();
                    QVERIFY2(novels::createSource(config) != nullptr, qPrintable(config.engine));
                }
            }
    }
};

QTEST_GUILESS_MAIN(Parsing)
#include "tst_parsing.moc"
