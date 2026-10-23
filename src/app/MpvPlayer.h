#pragma once

#include <QQuickFramebufferObject>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace sc {

struct MpvCore;

// The native video player: libmpv drawing through its render API straight
// into the Qt Quick scene (OpenGL; no frame ever goes through the CPU),
// with hardware decoding where the system has it. It plays one address
// with the headers its source needs, adds external subtitle and audio
// tracks, and reports where it is for resume, skips and auto-next.
// Builds without libmpv have the type but no player (available is false).
class MpvPlayer : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    // Built with libmpv and able to draw here (an OpenGL scene).
    Q_PROPERTY(bool available READ available CONSTANT)
    // What to play: an http(s) address, the headers to send with it
    // ({name: value}; User-Agent and Referer included), and options the
    // video's source asked for ({name: value}; only safe ones are used).
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QVariantMap headers READ headers WRITE setHeaders NOTIFY headersChanged)
    Q_PROPERTY(QVariantMap options READ options WRITE setOptions NOTIFY optionsChanged)
    // External tracks: [{url, lang}].
    Q_PROPERTY(QVariantList subtitles READ subtitles WRITE setSubtitles NOTIFY subtitlesChanged)
    Q_PROPERTY(QVariantList audio READ audio WRITE setAudio NOTIFY audioChanged)
    // Where the next source starts, in seconds.
    Q_PROPERTY(double startAt READ startAt WRITE setStartAt NOTIFY startAtChanged)
    // The picture height a stream with several (HLS variants) starts at:
    // the tallest at or under it, else the shortest; 0 for the best.
    Q_PROPERTY(int preferredHeight READ preferredHeight WRITE setPreferredHeight NOTIFY preferredHeightChanged)
    // Audio languages to pick first, as mpv's alang ("eng,en"); empty for
    // the stream's own default.
    Q_PROPERTY(QString audioLanguages READ audioLanguages WRITE setAudioLanguages NOTIFY audioLanguagesChanged)

    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    // Seconds ahead of the position that are already downloaded.
    Q_PROPERTY(double buffered READ buffered NOTIFY bufferedChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    // Waiting for the network (before the first frame or while playing).
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool ended READ ended NOTIFY endedChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    // Where subtitles sit, in % of the height from the top (100: their
    // usual place); raised while controls cover the bottom.
    Q_PROPERTY(int subtitlePosition READ subtitlePosition WRITE setSubtitlePosition NOTIFY subtitlePositionChanged)
    // How subtitles and the picture look (Settings → Anime): {subtitleScale,
    // subtitleOutline, subtitleColor, subtitleBox, sharpen: off|fast|hq}.
    // Subtitles with their own styling (ASS) keep it, apart from the size.
    Q_PROPERTY(QVariantMap look READ look WRITE setLook NOTIFY lookChanged)
    // [{id, type: video|audio|sub, title, lang, selected, external,
    // height (video), bitrate (an HLS variant's)}]
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    // The hardware decoder in use ("vaapi", "d3d11va", …), empty when the
    // video is decoded in software.
    Q_PROPERTY(QString hwdec READ hwdec NOTIFY hwdecChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit MpvPlayer(QQuickItem *parent = nullptr);
    ~MpvPlayer() override;

    // Whether this build plays through libmpv, and the scene draws with
    // OpenGL. main() calls setSceneSupported() once it knows the graphics
    // API.
    static bool supported();
    static void setSceneSupported(bool supported);

    Renderer *createRenderer() const override;

    bool available() const { return supported(); }
    QString source() const { return m_source; }
    void setSource(const QString &source);
    QVariantMap headers() const { return m_headers; }
    void setHeaders(const QVariantMap &headers);
    QVariantMap options() const { return m_options; }
    void setOptions(const QVariantMap &options);
    QVariantList subtitles() const { return m_subtitles; }
    void setSubtitles(const QVariantList &subtitles);
    QVariantList audio() const { return m_audio; }
    void setAudio(const QVariantList &audio);
    double startAt() const { return m_startAt; }
    void setStartAt(double seconds);
    int preferredHeight() const { return m_preferredHeight; }
    void setPreferredHeight(int height);
    QString audioLanguages() const { return m_audioLanguages; }
    void setAudioLanguages(const QString &languages);

    double position() const { return m_position; }
    double duration() const { return m_duration; }
    double buffered() const { return m_buffered; }
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    bool buffering() const { return m_buffering; }
    bool loaded() const { return m_loaded; }
    bool ended() const { return m_ended; }
    double volume() const { return m_volume; }
    void setVolume(double volume);
    bool muted() const { return m_muted; }
    void setMuted(bool muted);
    double speed() const { return m_speed; }
    void setSpeed(double speed);
    int subtitlePosition() const { return m_subtitlePosition; }
    void setSubtitlePosition(int position);
    QVariantMap look() const { return m_look; }
    void setLook(const QVariantMap &look);
    QVariantList tracks() const { return m_tracks; }
    QString hwdec() const { return m_hwdec; }
    QString error() const { return m_error; }

    // To an absolute position, or by an offset (both in seconds).
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void seekBy(double seconds);
    Q_INVOKABLE void togglePause();
    // Picks a track of a type ("audio", "sub", "video"); id 0 turns
    // subtitles off.
    Q_INVOKABLE void setTrack(const QString &type, int id);
    Q_INVOKABLE void stop();

    // From the renderer: it has its mpv render context, so videos can open.
    void rendererReady();

signals:
    void sourceChanged();
    void headersChanged();
    void optionsChanged();
    void subtitlesChanged();
    void audioChanged();
    void startAtChanged();
    void preferredHeightChanged();
    void audioLanguagesChanged();
    void positionChanged();
    void durationChanged();
    void bufferedChanged();
    void pausedChanged();
    void bufferingChanged();
    void loadedChanged();
    void endedChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void subtitlePositionChanged();
    void lookChanged();
    void tracksChanged();
    void hwdecChanged();
    void errorChanged();
    // The video ran to its end.
    void finished();

private:
    void load();
    void drainEvents();
    void setError(const QString &error);
    // The variant for preferredHeight, once a new file's tracks are known.
    void pickVariant();

    std::shared_ptr<MpvCore> m_core;
    QString m_source;
    QVariantMap m_headers;
    QVariantMap m_options;
    QVariantList m_subtitles;
    QVariantList m_audio;
    double m_startAt = 0;
    int m_preferredHeight = 0;
    QString m_audioLanguages;
    bool m_variantPending = false;
    double m_position = 0;
    double m_reportedPosition = -1;
    double m_duration = 0;
    double m_buffered = 0;
    bool m_paused = false;
    bool m_buffering = false;
    bool m_cachePause = false;
    bool m_seeking = false;
    bool m_loaded = false;
    bool m_ended = false;
    double m_volume = 100;
    bool m_muted = false;
    double m_speed = 1;
    int m_subtitlePosition = 100;
    QVariantMap m_look;
    QVariantList m_tracks;
    QString m_hwdec;
    QString m_error;
    bool m_loadQueued = false;
    bool m_rendererReady = false;
    // Playing from a server in the extension host (ext::holdHost).
    bool m_holdsHost = false;
    bool m_loadWaiting = false;
};

} // namespace sc
