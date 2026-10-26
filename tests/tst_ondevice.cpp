// Translating on the computer (OnDevice): ONNX Runtime with PaddleOCR and
// Opus-MT models. Needs the downloaded pack: SCANCORE_ONNX_LIB (the
// library) and SCANCORE_ONDEVICE_DIR (ocr/det.onnx, ocr/ko/{rec.onnx,
// inference.yml}, mt/ko-en/{encoder,decoder}.onnx, tokenizer.json,
// config.json); skipped without them.
#include "core/translation/LocalMt.h"
#include "app/LocalOcr.h"
#include "app/LocalUpscale.h"
#include "core/translation/Onnx.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QtTest>

using namespace sc;
using namespace sc::translation;

class OnDeviceTests : public QObject {
    Q_OBJECT
    QString m_dir;

private slots:
    void initTestCase() {
        m_dir = qEnvironmentVariable("SCANCORE_ONDEVICE_DIR");
        const auto library = qEnvironmentVariable("SCANCORE_ONNX_LIB");
        if (m_dir.isEmpty() || library.isEmpty())
            QSKIP("SCANCORE_ONDEVICE_DIR and SCANCORE_ONNX_LIB are not set");
        QString error;
        QVERIFY2(onnx::load(library, &error), qPrintable(error));
    }

    void readsCharacterLists() {
        QFile yaml(m_dir + QStringLiteral("/ocr/ko/inference.yml"));
        QVERIFY(yaml.open(QIODevice::ReadOnly));
        const auto characters = charactersFromYaml(yaml.readAll());
        QCOMPARE(characters.size(), 11945);
        QCOMPARE(characters.first(), QStringLiteral("ᄀ"));
        QVERIFY(characters.contains(QStringLiteral("'")));
        QVERIFY(characters.contains(QStringLiteral("가")));
    }

    void translatesKorean() {
        const Marian marian(m_dir + QStringLiteral("/mt/ko-en"));
        const auto ids = marian.encode(QStringLiteral("나는 학생입니다."));
        QVERIFY(ids.size() > 2);
        QCOMPARE(ids.last(), 0);
        const QList<QPair<QString, QString>> cases{{QStringLiteral("나는 학생입니다."), QStringLiteral("student")},
                                                   {QStringLiteral("오늘 날씨가 정말 좋네요."), QString()},
                                                   {QStringLiteral("도망쳐!"), QString()}};
        for (const auto &[source, word] : cases) {
            QElapsedTimer timer;
            timer.start();
            const auto out = marian.translate(source);
            qInfo("%s -> %s (%lld ms)", qPrintable(source), qPrintable(out), timer.elapsed());
            QVERIFY(!out.isEmpty());
            if (!word.isEmpty())
                QVERIFY2(out.contains(word, Qt::CaseInsensitive), qPrintable(out));
        }
        QCOMPARE(marian.translate(QStringLiteral("   ")), QString());
    }

    void readsKoreanText() {
        QFile yaml(m_dir + QStringLiteral("/ocr/ko/inference.yml"));
        QVERIFY(yaml.open(QIODevice::ReadOnly));
        const auto list = m_dir + QStringLiteral("/ocr/ko/chars.txt");
        QFile out(list);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(charactersFromYaml(yaml.readAll()).join(QLatin1Char('\n')).toUtf8() + '\n');
        out.close();
        const LocalOcr ocr(m_dir + QStringLiteral("/ocr/det.onnx"), m_dir + QStringLiteral("/ocr/ko/rec.onnx"), list);

        // Two lines in a bubble on a page, as a webtoon draws them.
        QImage page(800, 1200, QImage::Format_RGB32);
        page.fill(QColor(235, 230, 220));
        QPainter p(&page);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::white);
        p.setPen(QPen(Qt::black, 3));
        p.drawEllipse(QRect(150, 300, 500, 260));
        QFont font(QStringLiteral("Noto Sans CJK KR"));
        font.setPixelSize(40);
        font.setBold(true);
        p.setFont(font);
        p.drawText(QRect(150, 340, 500, 70), Qt::AlignCenter, QStringLiteral("오늘 날씨가"));
        p.drawText(QRect(150, 420, 500, 70), Qt::AlignCenter, QStringLiteral("정말 좋네요!"));
        p.end();

        QElapsedTimer timer;
        timer.start();
        const auto lines = ocr.lines(page);
        qInfo("%lld lines in %lld ms", qsizetype(lines.size()), timer.elapsed());
        QStringList texts;
        for (const auto &l : lines) {
            qInfo("  %s at %d,%d %dx%d", qPrintable(l.text), int(l.box.x()), int(l.box.y()), int(l.box.width()), int(l.box.height()));
            texts.append(l.text);
        }
        QVERIFY2(texts.join(QLatin1Char(' ')).contains(QStringLiteral("날씨가")), qPrintable(texts.join(QStringLiteral(" | "))));
        QVERIFY2(texts.join(QLatin1Char(' ')).contains(QStringLiteral("좋네요")), qPrintable(texts.join(QStringLiteral(" | "))));
    }

    // waifu2x on a page: twice the size, the same picture (SCANCORE_UPSCALE_MODEL).
    void upscalesPages() {
        const auto model = qEnvironmentVariable("SCANCORE_UPSCALE_MODEL");
        if (model.isEmpty())
            QSKIP("SCANCORE_UPSCALE_MODEL is not set");
        const Waifu2x waifu2x(model);
        QImage page(800, 1200, QImage::Format_RGB32);
        page.fill(QColor(235, 230, 220));
        QPainter p(&page);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::black, 4));
        p.setBrush(QColor(200, 60, 60));
        p.drawEllipse(QRect(120, 200, 560, 700));
        p.end();
        QElapsedTimer timer;
        timer.start();
        const auto big = waifu2x.upscale(page);
        qInfo("800x1200 -> %dx%d in %lld ms", big.width(), big.height(), timer.elapsed());
        QCOMPARE(big.size(), QSize(1600, 2400));
        // The same colours where nothing changes, inside and out.
        const QColor outside = big.pixelColor(40, 40), inside = big.pixelColor(800, 1100);
        QVERIFY2(std::abs(outside.red() - 235) < 6 && std::abs(outside.blue() - 220) < 6, qPrintable(outside.name()));
        QVERIFY2(std::abs(inside.red() - 200) < 6 && std::abs(inside.green() - 60) < 6, qPrintable(inside.name()));
        // Tile seams: a flat area has no step at a tile's edge (440 px).
        QVERIFY(std::abs(big.pixelColor(439, 40).red() - big.pixelColor(440, 40).red()) < 3);
        // Odd sizes (edge tiles of odd width or height), as real pages have.
        QImage oddPage(941, 567, QImage::Format_RGB32);
        oddPage.fill(QColor(235, 230, 220));
        const auto odd = waifu2x.upscale(oddPage);
        QCOMPARE(odd.size(), QSize(1882, 1134));
        QVERIFY(std::abs(odd.pixelColor(1880, 1132).red() - 235) < 8);
    }
};

QTEST_MAIN(OnDeviceTests)
#include "tst_ondevice.moc"
