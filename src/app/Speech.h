#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

class QTextToSpeech;

namespace sc {

// Reading aloud with the system's voices: Qt Speech on desktop (SAPI or
// WinRT, AVSpeechSynthesizer, speech-dispatcher), Android's own engine on
// phones (packaging/android/src/.../Speech.java). The novel reader queues a
// chapter's paragraphs with tags and hears back as each starts and ends, so
// it can follow along; Android's engine keeps reading the queue even while
// the app is in the background.
class Speech : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool speaking READ speaking NOTIFY changed)
    // [{id, name, language}], best for the app's language first.
    Q_PROPERTY(QVariantList voices READ voices NOTIFY voicesChanged)

public:
    static Speech *instance();
    static Speech *create(QQmlEngine *, QJSEngine *) { return instance(); }
    ~Speech() override;

    bool available() const { return m_available; }
    bool speaking() const { return !m_queued.isEmpty(); }
    QVariantList voices() const { return m_voices; }

    // Adds text to the queue; started(tag) and finished(tag) follow.
    Q_INVOKABLE void enqueue(const QString &text, int tag);
    // Empties the queue and stops at once.
    Q_INVOKABLE void stop();
    // rate and pitch are multipliers (1 = the voice's own); voice is an id
    // from voices, or empty for the system's choice.
    Q_INVOKABLE void configure(const QString &voice, double rate, double pitch);

    // From the platform engine (any thread).
    void engineReady(bool ok, const QVariantList &voices);
    void engineStarted(int chunk);
    void engineFinished(int chunk);
    void engineFailed(int chunk, const QString &message);

signals:
    void changed();
    void voicesChanged();
    void started(int tag);
    void finished(int tag);
    void failed(const QString &message);

private:
    explicit Speech(QObject *parent = nullptr);
    void ensureEngine();
    void sendChunk(int chunk, const QString &text);

    bool m_available = false;
    bool m_engineStarted = false;
    QVariantList m_voices;
    // Engine chunk id -> {tag, last chunk of the tag}.
    struct Chunk {
        int tag;
        bool last;
    };
    QHash<int, Chunk> m_queued;
    int m_nextChunk = 1;
#ifdef SCANCORE_HAVE_TTS
    QTextToSpeech *m_tts = nullptr;
    QHash<qsizetype, int> m_qtIds; // Qt's enqueue id -> chunk
    int m_current = 0;
#endif
};

} // namespace sc
