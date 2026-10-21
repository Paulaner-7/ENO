#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

class QQuickWindow;

namespace sc {

// Development hook: SCANCORE_PROFILE=1 (with start-up steps in main.cpp)
// also reports, while the app runs, each with the page on screen and how
// long since it opened:
// - frames whose rendering (sync to swap) took over 24 ms,
// - moments the GUI thread was busy for more than 33 ms, two frames
//   (JavaScript, layout, a blocking call),
// and how long each page took from being asked for to its first frame.
// Every 10 s a summary gives the frames drawn and how many were slow. Off,
// it costs nothing.
class Profiler : public QObject {
    Q_OBJECT

public:
    static bool enabled();
    static Profiler *instance();

    void attach(QQuickWindow *window);
    // The page now on screen ("comics", "pages/SourcePage.qml", ...).
    void setPlace(const QString &place);

private:
    explicit Profiler(QObject *parent = nullptr);
    void syncing();
    void frame();
    void tick();
    void summary();

    // Frames are counted on the render thread.
    QMutex m_mutex;
    QString m_place = QStringLiteral("start");
    QElapsedTimer m_placeClock;
    QElapsedTimer m_renderClock;
    bool m_waitingFirstFrame = false;
    QTimer m_watchdog;
    QElapsedTimer m_watchClock;
    QTimer m_summary;
    int m_frames = 0;
    int m_slow = 0;
    qint64 m_worst = 0;
    int m_stalls = 0;
    qint64 m_worstStall = 0;
};

} // namespace sc
