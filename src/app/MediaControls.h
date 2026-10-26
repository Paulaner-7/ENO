#pragma once

#include <QObject>
#include <QString>

namespace sc {

// The system's controls for what SCANCORE is playing without a screen of
// its own (reading aloud): the lock screen, the notification and
// headphones' buttons on Android (ReadAloudService.java), MPRIS on Linux
// (media keys, the desktop's player widget). Elsewhere it does nothing yet.
class MediaControls : public QObject {
    Q_OBJECT

public:
    static MediaControls *instance();

    // Shows (or updates) the controls, or with active false takes them away.
    void show(bool active, bool playing, const QString &title, const QString &subtitle, const QString &cover);

    // From the platform (any thread is fine; delivered on the GUI thread).
    void command(const QString &word);

signals:
    void playRequested();
    void pauseRequested();
    void toggleRequested();
    void nextRequested();
    void previousRequested();
    void stopRequested();

private:
    explicit MediaControls(QObject *parent = nullptr);

    bool m_active = false;
    bool m_playing = false;
    QString m_title;
    QString m_subtitle;
    QString m_cover;
    QObject *m_mpris = nullptr;
};

} // namespace sc
