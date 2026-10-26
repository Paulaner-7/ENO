pragma Singleton
import QtQuick
import Scancore

// Which video plays first (Settings → Anime): the picture height and the
// audio (the original or a dub) asked for. WatchPage ranks a source's
// videos with them; SCANCORE's player picks the stream variant and audio
// track inside one video with them.
QtObject {
    // Audio choices: mpv's language list for a stream's own tracks, and the
    // words a source's video names use for that language.
    readonly property var languages: [
        { value: "", label: qsTr("As the source plays it"), alang: "", words: [] },
        { value: "ja", label: qsTr("Japanese (original)"), alang: "jpn,ja,jp", words: ["japanese", "jap", "jpn"] },
        { value: "en", label: qsTr("English dub"), alang: "eng,en", words: ["english", "eng"] },
        { value: "es", label: qsTr("Spanish dub"), alang: "spa,es", words: ["spanish", "español", "espanol", "latino", "castellano", "esp"] },
        { value: "pt", label: qsTr("Portuguese dub"), alang: "por,pt", words: ["portuguese", "português", "portugues", "dublado", "ptbr"] },
        { value: "fr", label: qsTr("French dub"), alang: "fre,fra,fr", words: ["french", "français", "francais", "vf"] },
        { value: "de", label: qsTr("German dub"), alang: "ger,deu,de", words: ["german", "deutsch", "ger"] },
        { value: "it", label: qsTr("Italian dub"), alang: "ita,it", words: ["italian", "italiano", "ita"] },
        { value: "hi", label: qsTr("Hindi dub"), alang: "hin,hi", words: ["hindi"] }]
    readonly property var heights: [{ value: 0, label: qsTr("Best available") }, { value: 1080, label: "1080p" },
        { value: 720, label: "720p" }, { value: 480, label: "480p" }, { value: 360, label: "360p" }]

    readonly property string audio: App.settings.animeAudio || ""
    readonly property int height: Number(App.settings.animeQuality) || 0
    readonly property var language: languages.find(l => l.value === audio) || languages[0]
    readonly property string audioLanguages: language.alang

    function words(text) { return String(text || "").toLowerCase().split(/[\s\-_.,·:;()\[\]{}\/|+]+/).filter(w => w.length > 0) }

    // The picture height a video's name gives ("1080p", "720P", "4K",
    // "1920x1080"), or 0.
    function heightOf(text) {
        const t = String(text || "")
        let m = t.match(/(\d{3,4})\s*[pP]\b/)
        if (m) return Number(m[1])
        m = t.match(/\b\d{3,4}\s*[xX×]\s*(\d{3,4})\b/)
        if (m) return Number(m[1])
        if (/\b4k\b/i.test(t)) return 2160
        if (/\bfhd\b/i.test(t)) return 1080
        return 0
    }

    // How a video's name fits the audio asked for: 1 it does, -1 it is the
    // other kind (a dub when the original was asked for, or a subbed one
    // when a dub was), 0 it does not say.
    function audioFit(text) {
        if (!audio) return 0
        const w = words(text)
        const dub = w.some(x => ["dub", "dubs", "dubbed", "dublado", "doblado", "doblaje", "vf"].indexOf(x) >= 0)
        const sub = w.some(x => ["sub", "subs", "subbed", "softsub", "hardsub", "raw", "vostfr", "legendado", "subtitulado"].indexOf(x) >= 0)
        if (audio === "ja") return dub ? -1 : sub ? 1 : 0
        if (language.words.some(x => w.indexOf(x) >= 0)) return 1
        if (dub) return audio === "en" ? 1 : 0
        return sub ? -1 : 0
    }

    // Ranks a video by its name: the audio first, then the height (the
    // tallest at or under the one asked for, else the closest above; with
    // no height asked, the tallest).
    function score(text, preferred) {
        let s = audioFit(text) * 1000
        const h = heightOf(text)
        if (h > 0)
            s += height > 0 ? (h <= height ? 500 - (height - h) / 10 : 400 - (h - height) / 10) : h / 10
        return s + (preferred ? 5 : 0)
    }
}
