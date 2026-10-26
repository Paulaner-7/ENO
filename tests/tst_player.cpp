// SCANCORE's player (MpvPlayer) in a real scene: going to the next episode
// over and over, while the picture is drawn, at any moment (before the
// last one has loaded, or as the picture's fit changes). Each new video
// must play and be drawn, and the interface must never wait on mpv (2.12.x:
// after Next the picture froze with the app while the sound played on).
// Needs libmpv and an OpenGL scene; skipped without them.
#include "app/MpvPlayer.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSGRendererInterface>
#include <QTimer>
#include <QtTest>

#include <atomic>

using namespace sc;

class PlayerTests : public QObject {
    Q_OBJECT

private slots:
    void nextEpisodeKeepsDrawing() {
        if (!MpvPlayer::supported())
            QSKIP("built without libmpv");
        QQuickWindow window;
        window.resize(320, 180);
        window.show();
        if (!QTest::qWaitForWindowExposed(&window) || window.rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
            QSKIP("no OpenGL scene here");

        auto *player = new MpvPlayer(window.contentItem());
        player->setSize(QSizeF(320, 180));
        player->setMuted(true);
        player->setVolume(0);

        // Frames the scene's thread finished, and the longest the
        // interface's thread went without coming back to its event loop.
        std::atomic<int> frames{0};
        connect(&window, &QQuickWindow::afterRendering, &window, [&frames] { ++frames; }, Qt::DirectConnection);
        QElapsedTimer sinceTick;
        sinceTick.start();
        qint64 longestGap = 0;
        QTimer tick;
        tick.setInterval(5);
        connect(&tick, &QTimer::timeout, this, [&] {
            longestGap = std::max(longestGap, sinceTick.restart());
        });
        tick.start();

        const QString videos[] = {QUrl::fromLocalFile(QStringLiteral(SCANCORE_SOURCE_DIR "/tests/fixtures/video/a.mp4")).toString(),
                                  QUrl::fromLocalFile(QStringLiteral(SCANCORE_SOURCE_DIR "/tests/fixtures/video/b.mp4")).toString()};
        const QString fits[] = {QStringLiteral("fit"), QStringLiteral("stretch"), QStringLiteral("crop")};
        auto *random = QRandomGenerator::global();
        for (int episode = 0; episode < 12; ++episode) {
            player->setSource(videos[episode % 2]);
            if (episode % 3 == 2) {
                // Next again before this one has loaded.
                QTest::qWait(int(random->bounded(10, 80)));
                player->setSource(videos[(episode + 1) % 2]);
            }
            if (episode % 4 == 1)
                player->setLook({{QStringLiteral("fit"), fits[episode % 3]}});
            QTRY_VERIFY2_WITH_TIMEOUT(player->loaded() && player->position() > 0.3, qPrintable(QStringLiteral("episode %1 did not play: %2").arg(episode).arg(player->error())), 10000);
            // Drawn after it started, too.
            const int before = frames;
            QTRY_VERIFY_WITH_TIMEOUT(frames > before + 3, 5000);
            QTest::qWait(int(random->bounded(0, 400)));
        }
        player->setSource({});
        QTest::qWait(200);
        tick.stop();
        // A slow frame now and then is fine; waiting on mpv is not.
        QVERIFY2(longestGap < 1500, qPrintable(QStringLiteral("the interface waited %1 ms").arg(longestGap)));
        delete player;
        // The renderer goes on the scene's thread; the window must still draw.
        const int before = frames;
        window.update();
        QTRY_VERIFY_WITH_TIMEOUT(frames > before, 5000);
    }
};

int main(int argc, char **argv) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    PlayerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_player.moc"
