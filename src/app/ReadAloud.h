#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace sc {

class Task;

// Reading a novel aloud, as one session that does not need the reader on
// screen (audiobook mode): it holds the book, the chapter and the sentence
// being spoken, feeds them to the voice (Speech) a sentence at a time with
// the pauses the user chose, goes on to the next chapter by itself, keeps
// the reading position and stops for a sleep timer. The novel reader shows
// and follows it (highlighting the sentence); on Android a media service
// keeps it going with the screen off, with controls on the lock screen and
// in the notification (ReadAloudService.java); elsewhere the mini player
// and the system's media keys (MPRIS on Linux) do.
//
// Settings (App): ttsVoice (default voice), ttsVoices ({language: voice}),
// ttsRate, ttsPitch, ttsParagraphPause and ttsSentencePause (ms),
// ttsContinue (on to the next chapter), ttsFollow, ttsHighlight.
class ReadAloud : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap series READ series NOTIFY sessionChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY sessionChanged)
    Q_PROPERTY(int chapterIndex READ chapterIndex NOTIFY chapterChanged)
    Q_PROPERTY(QString chapterTitle READ chapterTitle NOTIFY chapterChanged)
    // The chapter's paragraphs, as the reader has them: [{kind, text}].
    Q_PROPERTY(QVariantList blocks READ blocks NOTIFY chapterChanged)
    // The sentence being spoken: its paragraph (-1: the chapter's title)
    // and where it is in that paragraph's text.
    Q_PROPERTY(int block READ block NOTIFY positionChanged)
    Q_PROPERTY(int sentenceStart READ sentenceStart NOTIFY positionChanged)
    Q_PROPERTY(int sentenceLength READ sentenceLength NOTIFY positionChanged)
    // 0 to 1 through the chapter.
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    // "off", "minutes" or "chapter"; with minutes, when it stops.
    Q_PROPERTY(QString sleepMode READ sleepMode NOTIFY sleepChanged)
    Q_PROPERTY(qint64 sleepEndsAt READ sleepEndsAt NOTIFY sleepChanged)

public:
    static ReadAloud *instance();
    static ReadAloud *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool active() const { return !m_series.isEmpty(); }
    bool playing() const { return m_playing; }
    bool loading() const { return m_task != nullptr; }
    QVariantMap series() const { return m_series; }
    QVariantList chapters() const { return m_chapters; }
    int chapterIndex() const { return m_index; }
    QString chapterTitle() const { return m_title; }
    QVariantList blocks() const { return m_blocks; }
    int block() const;
    int sentenceStart() const;
    int sentenceLength() const;
    double progress() const;
    QString sleepMode() const { return m_sleepMode; }
    qint64 sleepEndsAt() const { return m_sleepEndsAt; }

    // Starts reading `chapters[index]` of `series`. With the chapter's
    // blocks (the reader has them) from paragraph `fromBlock` (-1: its
    // title); without, the chapter is loaded first and read from the start
    // (or from `fromBlock`).
    Q_INVOKABLE void start(const QVariantMap &series, const QVariantList &chapters, int index,
                           const QVariantList &blocks = {}, int fromBlock = -1, const QString &title = QString());
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    // Ends the session.
    Q_INVOKABLE void stop();
    // By sentences (negative: back), or to a paragraph.
    Q_INVOKABLE void skip(int sentences);
    Q_INVOKABLE void seekBlock(int block);
    Q_INVOKABLE void skipChapter(int by);
    // minutes > 0: stop after that long; 0: off; -1: at the end of this
    // chapter.
    Q_INVOKABLE void setSleep(int minutes);
    // The voice for a language (the book's), from ttsVoices or ttsVoice.
    Q_INVOKABLE QString voiceFor(const QString &language) const;
    // Picks the voice for one language ("" = the default voice).
    Q_INVOKABLE void setVoiceFor(const QString &language, const QString &voice);

    // Splits text into sentences: [{start, length}] (tests use it).
    static QList<QPair<int, int>> sentences(const QString &text);

signals:
    void stateChanged();
    void sessionChanged();
    void chapterChanged();
    void positionChanged();
    void sleepChanged();
    void failed(const QString &message);
    // The sleep timer stopped reading.
    void slept();

private:
    explicit ReadAloud(QObject *parent = nullptr);

    struct Sentence {
        int block;
        int start;
        int length;
        QString text;
    };
    void setChapter(const QVariantMap &chapter, int fromBlock);
    void loadChapter(int index, int fromBlock);
    void feed();
    void spoken(int tag);
    void began(int tag);
    void chapterDone();
    void configureVoice();
    void saveProgress(bool force);
    void setPlaying(bool playing);
    void updatePlatform();
    QString language() const;
    int pauseAfter(int sentence) const;

    QVariantMap m_series;
    QVariantList m_chapters;
    int m_index = -1;
    QString m_title;
    QVariantList m_blocks;
    QList<Sentence> m_sentences;
    // The sentence being spoken, and the next one to hand to the voice.
    int m_current = 0;
    int m_next = 0;
    bool m_playing = false;
    // Each start gets a new generation, so the voice's late answers about
    // an old queue are ignored.
    int m_generation = 0;
    QPointer<Task> m_task;
    QTimer m_pause;
    QTimer m_sleep;
    QString m_sleepMode = QStringLiteral("off");
    qint64 m_sleepEndsAt = 0;
    QElapsedTimer m_lastSave;
};

} // namespace sc
