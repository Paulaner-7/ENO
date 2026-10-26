#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QVariantList>

class QProcess;

namespace sc {

class Task;

// Neural voices on computers: Piper (github.com/rhasspy/piper, MIT), an
// optional download kept apart from SCANCORE (its phonemizer is GPL-3): the
// program once (about 25 MB), then each voice (about 60 MB, from the
// piper-voices catalogue). Piper runs as its own process and writes each
// paragraph to a WAV file; an audio-only libmpv plays them in order. Speech
// uses it for voices whose id starts with "piper:".
class Piper : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // This computer can have neural voices (Piper is built for it, and the
    // app has libmpv to play them).
    Q_PROPERTY(bool available READ available CONSTANT)
    // [{id: "piper:<key>", key, name, language, quality}]
    Q_PROPERTY(QVariantList voices READ voices NOTIFY voicesChanged)

public:
    static Piper *instance();
    static Piper *create(QQmlEngine *, QJSEngine *) { return instance(); }
    ~Piper() override;

    bool available() const;
    QVariantList voices() const { return m_voices; }

    // The catalogue: [{key, name, language, languageName, quality, size,
    // installed}], the app's language first.
    Q_INVOKABLE sc::Task *catalogue();
    // Downloads the program (the first time) and the voice; the task's
    // partial result is what it is doing ({status}).
    Q_INVOKABLE sc::Task *install(const QString &key);
    Q_INVOKABLE void remove(const QString &key);

    // Speaking (Speech): one voice at a time; rate is a multiplier.
    void setVoice(const QString &key);
    void setRate(double rate);
    void speak(int chunk, const QString &text);
    void stop();

signals:
    void voicesChanged();
    void started(int chunk);
    void finished(int chunk);
    void failed(int chunk, const QString &message);

private:
    explicit Piper(QObject *parent = nullptr);
    void scanVoices();
    bool ensureProcess();
    bool ensurePlayer();
    void pump();
    void readOutput();
    void drainPlayer();
    void fail(const QString &message);

    QVariantList m_voices;
    QString m_voice;
    double m_rate = 1;
    QProcess *m_process = nullptr;
    QString m_processVoice;
    // What is waiting for Piper, what Piper is saying (in order), and what
    // has been handed to the player (playlist entry -> chunk).
    struct Line {
        int chunk;
        QString text;
    };
    QList<Line> m_waiting;
    QList<int> m_synth;
    QHash<qint64, int> m_entries;
    QSet<qint64> m_startedEarly;
    // Chunks in Piper or the player, not yet finished.
    QSet<int> m_outstanding;
    // Each stop() starts a new generation; files of older ones are dropped.
    int m_generation = 0;
    void *m_mpv = nullptr;
};

} // namespace sc
