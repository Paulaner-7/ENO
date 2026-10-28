#include "core/Aes.h"
#include "core/Epub.h"
#include "core/Text.h"
#include "core/translation/Cloud.h"
#include "core/translation/Layout.h"
#include "core/translation/Subtitles.h"
#include "core/Http.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "sources/local/LocalSources.h"

#include <QBuffer>
#include <QtEndian>

#include <QTemporaryDir>
#include <QUrl>

#include <QTest>

using namespace sc;
using namespace sc::translation;

// Text helpers that need no network or database.
class TextTests : public QObject {
    Q_OBJECT

    static QStringList split(const QString &text) {
        QStringList out;
        for (const auto &[start, length] : sentenceSpans(text))
            out.append(text.mid(start, length));
        return out;
    }

private slots:

    // Reading aloud speaks a sentence at a time.
    void sentences() {
        QCOMPARE(split(QStringLiteral("He ran. She stayed! Why?  ")),
                 (QStringList{QStringLiteral("He ran."), QStringLiteral("She stayed!"), QStringLiteral("Why?")}));
        // Quotes stay with their sentence; a lone ellipsis is not one.
        QCOMPARE(split(QStringLiteral("“Wait.” He turned. …")),
                 (QStringList{QStringLiteral("“Wait.”"), QStringLiteral("He turned.")}));
        // Chinese and Japanese full stops.
        QCOMPARE(split(QStringLiteral("他走了。她留下！为什么？")),
                 (QStringList{QStringLiteral("他走了。"), QStringLiteral("她留下！"), QStringLiteral("为什么？")}));
        QCOMPARE(split(QString()).size(), 0);
        QCOMPARE(split(QStringLiteral("***")).size(), 0);
        // Positions point into the original text.
        const auto spans = sentenceSpans(QStringLiteral("  One. Two."));
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans[0].first, 2);
        QCOMPARE(spans[1].first, 7);
        QCOMPARE(spans[1].second, 4);
    }

    // CryptoJS's AES, as sites use it for chapter lists (vector made with
    // OpenSSL's own AES-256-CBC and key derivation).
    void decryptsCryptoJs() {
        const auto data = QByteArray::fromBase64(
            "NOU6l25NzRPiNPCe9QKc2dUf6ctPLw7uP7yM14/ZyFnJBQi7f98Q40HAU/wU1NuDLd430sJ9ufOvGBO4Bw8ebyoYp6fOZlqHB5+Weu4czEw=");
        const auto out = cryptoJsDecrypt("not-the-site-key", QByteArray::fromHex("0123456789abcdef"), data);
        QVERIFY(out.has_value());
        QCOMPARE(QString::fromUtf8(*out), QStringLiteral("[{\"slug\":\"1-abc\",\"number\":\"1\"},{\"slug\":\"2-def\",\"number\":\"2\"}] — capítulo"));
        // The wrong key: the padding gives it away.
        QVERIFY(!cryptoJsDecrypt("wrong", QByteArray::fromHex("0123456789abcdef"), data).has_value());
    }

    // Page translation: lines found on a page become bubbles.
    void groupsLinesIntoBubbles() {
        using namespace translation;
        // Two bubbles of horizontal Korean text, far apart.
        const QList<TextLine> lines{{QRectF(100, 100, 200, 30), QStringLiteral("안녕하세요,")},
                                    {QRectF(110, 135, 180, 30), QStringLiteral("반갑습니다.")},
                                    {QRectF(500, 600, 150, 30), QStringLiteral("뭐라고?")}};
        const auto blocks = groupLines(lines, QStringLiteral("ko"));
        QCOMPARE(blocks.size(), 2);
        QCOMPARE(blocks[0].text, QStringLiteral("안녕하세요, 반갑습니다."));
        QCOMPARE(blocks[0].box, QRectF(100, 100, 200, 65));
        QCOMPARE(blocks[1].text, QStringLiteral("뭐라고?"));
        // Japanese columns read right to left, joined without spaces.
        const QList<TextLine> columns{{QRectF(200, 100, 30, 150), QStringLiteral("ですか")},
                                      {QRectF(240, 100, 30, 160), QStringLiteral("本当")}};
        const auto ja = groupLines(columns, QStringLiteral("ja"));
        QCOMPARE(ja.size(), 1);
        QVERIFY(ja[0].vertical);
        QCOMPARE(ja[0].text, QStringLiteral("本当ですか"));
        // A word broken over two lines joins again.
        const auto en = groupLines({{QRectF(0, 0, 100, 20), QStringLiteral("a won-")}, {QRectF(0, 24, 100, 20), QStringLiteral("derful day")}},
                                   QStringLiteral("en"));
        QCOMPARE(en.size(), 1);
        QCOMPARE(en.first().text, QStringLiteral("a wonderful day"));
    }

    // The services, answered here: what is sent and how answers are read.
    void cloudEngines() {
        QList<Request> seen;
        Http::setTestHandler([&](const Request &request) -> std::optional<Response> {
            seen.append(request);
            Response r;
            r.status = 200;
            const auto host = request.url.host();
            if (host == QLatin1String("api-free.deepl.com"))
                r.body = R"({"translations":[{"text":"Hello"},{"text":"What?"}]})";
            else if (host == QLatin1String("translation.googleapis.com"))
                r.body = R"({"data":{"translations":[{"translatedText":"Hello"}]}})";
            else if (host == QLatin1String("llm.test"))
                r.body = QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"message", QJsonObject{{"content", "```json\n[\"Hi\", \"No way\"]\n```"}}}}}}})
                             .toJson();
            else if (host == QLatin1String("vision.googleapis.com"))
                r.body = R"({"responses":[{"fullTextAnnotation":{"pages":[{"blocks":[{"paragraphs":[
                    {"boundingBox":{"vertices":[{"x":10,"y":10},{"x":110,"y":10},{"x":110,"y":40},{"x":10,"y":40}]},
                     "words":[{"symbols":[{"text":"안"},{"text":"녕","property":{"detectedBreak":{"type":"SPACE"}}}]},
                              {"symbols":[{"text":"하"},{"text":"세"},{"text":"요"}]}]}]}]}]}}]})";
            else if (host == QLatin1String("translate.googleapis.com"))
                r.body = R"([[["Ciao\n","Hello\n"],["Che cosa?", "What?"]],null,"en"])";
            else
                return std::nullopt;
            return r;
        });
        using namespace translation;
        QCOMPARE(gtx({QStringLiteral("Hello"), QStringLiteral("What?")}, QStringLiteral("en"), QStringLiteral("it")),
                 (QStringList{QStringLiteral("Ciao"), QStringLiteral("Che cosa?")}));
        QCOMPARE(deepl({QStringLiteral("안녕"), QStringLiteral("뭐?")}, QStringLiteral("ko"), QStringLiteral("en"), QStringLiteral("k:fx")),
                 (QStringList{QStringLiteral("Hello"), QStringLiteral("What?")}));
        QCOMPARE(seen.last().headers.value(0).second, QByteArray("DeepL-Auth-Key k:fx"));
        const auto deeplBody = QJsonDocument::fromJson(seen.last().body).object();
        QCOMPARE(deeplBody.value(QStringLiteral("target_lang")).toString(), QStringLiteral("EN-US"));
        QCOMPARE(deeplBody.value(QStringLiteral("source_lang")).toString(), QStringLiteral("KO"));

        QCOMPARE(google({QStringLiteral("안녕")}, QString(), QStringLiteral("en"), QStringLiteral("g")), QStringList{QStringLiteral("Hello")});
        QVERIFY(seen.last().url.query().contains(QStringLiteral("key=g")));

        QCOMPARE(chatModel({QStringLiteral("안녕"), QStringLiteral("말도 안 돼")}, QStringLiteral("ko"), QStringLiteral("en"),
                           QStringLiteral("http://llm.test/v1/"), QString(), QStringLiteral("m")),
                 (QStringList{QStringLiteral("Hi"), QStringLiteral("No way")}));
        QCOMPARE(seen.last().url.path(), QStringLiteral("/v1/chat/completions"));

        const auto blocks = googleVision("jpeg", QStringLiteral("ko"), QStringLiteral("g"));
        QCOMPARE(blocks.size(), 1);
        QCOMPARE(blocks[0].text, QStringLiteral("안녕 하세요"));
        QCOMPARE(blocks[0].box, QRectF(10, 10, 100, 30));

        // No key: a message saying where to put it, nothing sent.
        const auto before = seen.size();
        QVERIFY_THROWS_EXCEPTION(Error, deepl({QStringLiteral("x")}, {}, QStringLiteral("en"), {}));
        QCOMPARE(seen.size(), before);
        Http::setTestHandler({});
    }

    // WebVTT, SubRip and ASS tracks all become cues with their text and
    // times, and a WebVTT track a browser can play.
    void subtitleTracks() {
        const auto vtt = parseSubtitles(QByteArrayLiteral(
            "WEBVTT\n\n"
            "NOTE a comment\n\n"
            "00:00:01.500 --> 00:00:03.000 align:start\n"
            "<v Roger>Hello &amp; welcome</v>\n\n"
            "00:00:04.000 --> 00:00:05.250\n"
            "Second line\n"
            "on two lines"));
        QCOMPARE(vtt.size(), 2);
        QCOMPARE(vtt[0].start, 1500);
        QCOMPARE(vtt[0].end, 3000);
        QCOMPARE(vtt[0].text, QStringLiteral("Hello & welcome"));
        QCOMPARE(vtt[1].start, 4000);
        QCOMPARE(vtt[1].end, 5250);
        QCOMPARE(vtt[1].text, QStringLiteral("Second line\non two lines"));

        const auto srt = parseSubtitles(QByteArrayLiteral(
            "1\n"
            "00:00:01,000 --> 00:00:02,500\n"
            "<i>Hi</i> &lt;there&gt;\n\n"
            "2\n"
            "00:00:03,000 --> 00:00:04,000\n"
            "Two\n"
            "lines"));
        QCOMPARE(srt.size(), 2);
        QCOMPARE(srt[0].text, QStringLiteral("Hi <there>"));
        QCOMPARE(srt[1].text, QStringLiteral("Two\nlines"));

        const auto ass = parseSubtitles(QByteArrayLiteral(
            "[Script Info]\n"
            "Title: Test\n\n"
            "[Events]\n"
            "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
            "Dialogue: 0,0:00:01.00,0:00:01.50,Signs,,0,0,0,,{\\p1}m 0 0 l 100 0 100 100 0 100{\\p0}\n"
            "Dialogue: 0,0:00:01.50,0:00:02.00,Default,,0,0,0,,{\\an8}Hello, world\\NSecond line\n"
            "Dialogue: 0,0:00:02.00,0:00:03.25,Default,,0,0,0,,{\\an8}Hello, world\\NSecond line\n"));
        QCOMPARE(ass.size(), 1);
        QCOMPARE(ass[0].start, 1500);
        QCOMPARE(ass[0].end, 3250);
        QCOMPARE(ass[0].text, QStringLiteral("Hello, world\nSecond line"));

        const auto written = toWebVtt(srt);
        QVERIFY(written.startsWith(QStringLiteral("WEBVTT\n\n")));
        QVERIFY(written.contains(QStringLiteral("00:00:01.000 --> 00:00:02.500")));
        const auto again = parseSubtitles(written.toUtf8());
        QCOMPARE(again.size(), 2);
        QCOMPARE(again[1].text, QStringLiteral("Two\nlines"));

        QCOMPARE(plainSubtitleText(QStringLiteral("Two\nlines  here")), QStringLiteral("Two lines here"));
        // Nothing that is not a track.
        QCOMPARE(parseSubtitles(QByteArrayLiteral("<html>no subtitles</html>")).size(), 0);
    }

    // Exported novels open in SCANCORE's own EPUB reader again, chapters,
    // titles and text intact, markup that is not XHTML written as text.
    void epubRoundTrip() {
        EpubBook book;
        book.id = QStringLiteral("novel:test:1");
        book.title = QStringLiteral("The Lantern & the Hill");
        book.author = QStringLiteral("Test Author");
        book.chapters.append({QStringLiteral("One"),
                              {QVariantMap{{"kind", "paragraph"}, {"text", "Plain text."}, {"html", "Plain <i>text</i>."}},
                               QVariantMap{{"kind", "break"}},
                               QVariantMap{{"kind", "paragraph"}, {"text", "Broken < markup"}, {"html", "Broken <b>markup"}}}});
        book.chapters.append({QStringLiteral("Two"), {QVariantMap{{"kind", "paragraph"}, {"text", "Second."}, {"html", "Second."}}}});
        const auto bytes = writeEpub(book);
        QVERIFY(bytes.mid(30, 8) == "mimetype");
        QTemporaryDir dir;
        const auto path = dir.filePath(QStringLiteral("book.epub"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();

        novels::SourceConfig config;
        config.key = QStringLiteral("local");
        const auto source = local::createNovelSource(config);
        QList<novels::ChapterRef> chapters;
        const auto read = source->book(QUrl::fromLocalFile(path).toString(), chapters);
        QCOMPARE(read.title, book.title);
        QCOMPARE(chapters.size(), 2);
        QString title;
        const auto blocks = source->chapter(chapters[0].url, title);
        QCOMPARE(title, QStringLiteral("One"));
        QStringList texts;
        for (const auto &b : blocks)
            texts.append(b.text);
        QVERIFY2(texts.contains(QStringLiteral("Plain text.")), qPrintable(texts.join(" | ")));
        QVERIFY2(texts.contains(QStringLiteral("Broken < markup")), qPrintable(texts.join(" | ")));
    }
};

QTEST_GUILESS_MAIN(TextTests)
#include "tst_text.moc"
