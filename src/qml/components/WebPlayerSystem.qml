import QtQuick
import QtQuick.Window
import QtWebView
import Scancore

// Player in the system's web view: Android WebView on phones, WebView2 on
// Windows and WKWebView on macOS. These play the H.264 episodes that the Qt
// WebEngine in Qt's own packages cannot. Like any native view it is drawn
// above everything Qt renders, so nothing can overlap it.
//
// Phones load the player inside a small page of ours (PlayerPage), as
// WebPlayer does, and read its time through getTime/seek messages; taps go
// to the player's own controls. The page reports back through
// its title, never through runJavaScript callbacks: on Android, Qt WebView
// runs those callbacks on the Android UI thread, alongside the QML engine on
// Qt's own thread, which crashes the app (a SIGSEGV in the JavaScript engine,
// often right after a tap). Title changes arrive as a signal, which Qt queues
// onto its own thread.
//
// Desktops open the player page itself and read its video element. WebView2
// cannot give a page of ours a real address, and inside a page without one
// the player cannot use its storage and never starts.
Item {
    id: root
    property string url
    readonly property bool direct: App.platform !== "android"
    readonly property bool isFs: root.Window.window !== null && root.Window.window.visibility === Window.FullScreen
    readonly property string subLangPref: App.settings.animeSubLang || "it"
    readonly property string subQualityPref: App.settings.animeSubQuality || "high"
    readonly property string videoQualityPref: App.settings.animeVideoQuality || "max"
    // Settings → Anime: a track in another language is translated here
    // (Translator) and shown over the video.
    readonly property bool translateSubs: App.settings.animeTranslateSubs === true
    property var subtitleTracks: []
    property string subtitleSignature: ""
    property string currentSubtitleUrl: ""
    property string translatingSubtitle: ""
    property string failedSubtitle: ""
    property bool subtitleTranslationStarted: false
    property bool subtitleToastShown: false
    property real subToastUntil: 0
    property string subToastMsg: ""
    property var subtitleTask: null
    property var translatedCues: []

    // Top bar & playback context (injected inside WKWebView on desktop)
    property string mediaTitle: ""
    property string episodeLine: ""
    property bool hasPrev: false
    property bool hasNext: false
    property var streams: []
    property string currentUrl: ""
    property bool chromeShown: true
    property real resumeAt: 0
    property var skips: []
    property var activeSkip: null
    property var episodesList: []
    property int currentIndex: 0
    property real videoTimeMs: 0
    readonly property string activeTranslatedText: {
        if (!translateSubs || !translatedCues || translatedCues.length === 0) return ""
        const ms = videoTimeMs
        const lines = []
        for (let i = 0; i < translatedCues.length; ++i) {
            const c = translatedCues[i]
            if (ms >= c.start && ms <= c.end && c.text) {
                if (lines.indexOf(c.text) < 0) lines.push(c.text)
            }
        }
        return lines.join("\n")
    }

    readonly property string streamsJson: JSON.stringify(streams || [])
    readonly property string activeSkipJson: JSON.stringify(activeSkip || null)
    readonly property string episodesJson: {
        const list = []
        for (let i = 0; i < (episodesList ? episodesList.length : 0); ++i) {
            const ep = episodesList[i]
            list.push({ i: i, n: ep.number, t: ep.title || "" })
        }
        return JSON.stringify(list)
    }

    signal time(real seconds, real total, string status)
    // The video was tapped (phones).
    signal interacted()
    // Desktop: a key pressed while the page had focus: {key, ctrl, alt, shift, meta}.
    signal key(var press)
    // The player's own codec support.
    signal unsupported()
    signal toggleFullScreen()
    signal backClicked()
    signal prevClicked()
    signal nextClicked()
    signal serverPicked(string url, string quality)
    signal skipClicked(real toSeconds)
    signal episodeSelected(int index)
    signal italianMissing()
    // The player's track was translated and is now shown.
    signal subtitleTranslated(string label)
    signal subtitleTranslationFailed(string error)

    property int taps: 0

    function seek(seconds) {
        if (direct) {
            const js = "(function(){"
                + "var t = " + Number(seconds) + ";"
                + "try { window.postMessage({command:'seek',value:t},'*'); } catch(_) {}"
                + "try {"
                + "  var art = window.Artplayer && window.Artplayer.instances && window.Artplayer.instances[0];"
                + "  if (art) {"
                + "    art.seek = t; art.currentTime = t;"
                + "    if (art.video) art.video.currentTime = t;"
                + "  }"
                + "} catch(_) {}"
                + "try {"
                + "  var v = document.querySelector('video');"
                + "  if (v) v.currentTime = t;"
                + "} catch(_) {}"
                + "})()"
            web.runJavaScript(js)
        } else {
            web.runJavaScript("window.scSeek && window.scSeek(" + Number(seconds) + ")")
        }
    }

    function syncFullscreen() {
        if (direct && web) {
            web.runJavaScript("window.postMessage({zenFullscreenState:" + (isFs ? "true" : "false") + "},'*')")
        }
    }

    // The language of a subtitle track as its name gives it ("English",
    // "ita", "Español"); "" when the name does not say.
    function subtitleLanguage(name) {
        const n = String(name || "").toLowerCase()
        if (/ita|italian|italiano/.test(n)) return "it"
        if (/eng|english/.test(n)) return "en"
        if (/jpn|japanese|日本語/.test(n)) return "ja"
        if (/spa|spanish|español|castellano/.test(n)) return "es"
        if (/fre|french|français|francais/.test(n)) return "fr"
        if (/ger|german|deutsch/.test(n)) return "de"
        if (/por|portuguese|português|portugues/.test(n)) return "pt"
        if (/rus|russian|русский/.test(n)) return "ru"
        if (/kor|korean|한국어/.test(n)) return "ko"
        if (/chi|chinese|中文|简体|繁體/.test(n)) return "zh"
        if (/ara|arabic|العربية/.test(n)) return "ar"
        return ""
    }

    // A track's address without the CDN host: the same file when the host
    // rotates during playback.
    function subtitleKey(url) {
        const address = String(url || "")
        const start = address.indexOf("://")
        const slash = start < 0 ? -1 : address.indexOf("/", start + 3)
        return slash >= 0 ? address.slice(slash) : address
    }

    // A track worth translating: full dialogue over signs and title cards.
    function subtitleScore(track) {
        const n = String(track.name || "").toLowerCase()
        const u = String(track.url || "").toLowerCase()
        let score = 0
        if (/full|dialog|complete/.test(n)) score += 4
        if (/subtitle/.test(n)) score += 1
        if (/eng|english/.test(n)) score += 2
        if (/\.(srt|vtt)(\?|$)/.test(u)) score += 0.25
        if (/forced|dubtitle|sdh|cc\b/.test(n)) score -= 4
        if (/sign|song|karaoke|\bop\b|\bed\b/.test(n)) score -= 4
        if (/episode name|title|credit/.test(n)) score -= 4
        return score
    }

    // The track to translate: full dialogue first, the one playing a little
    // beyond a tie. Null when the player already has Italian: nothing to do.
    function pickSubtitle(tracks, current) {
        const list = (tracks || []).filter(t => t && t.url)
        if (list.length === 0 || list.some(t => subtitleLanguage(t.name) === "it")) return null
        let best = null
        let bestScore = -1000
        for (const t of list) {
            const score = subtitleScore(t) + (subtitleKey(t.url) === subtitleKey(current) ? 0.5 : 0)
            if (score > bestScore) {
                bestScore = score
                best = t
            }
        }
        return best
    }

    // The probe's tracks: translate the chosen one once per page or track.
    function considerSubtitles(tracks, current) {
        currentSubtitleUrl = current || ""
        const list = []
        for (const t of (tracks || []))
            if (t && t.url) list.push({ name: t.name || "", url: t.url })
        const signature = JSON.stringify(list.map(t => ({ name: t.name, key: subtitleKey(t.url) })))
        if (signature !== subtitleSignature) {
            subtitleSignature = signature
            subtitleTracks = list
        }
        if (!translateSubs || list.length === 0 || !visible) return
        const track = pickSubtitle(list, currentSubtitleUrl)
        if (!track || subtitleKey(track.url) === translatingSubtitle || subtitleKey(track.url) === failedSubtitle) return
        translateSubtitle(track)
    }

    function translateSubtitle(track) {
        if (!track || !track.url || (subtitleTask && subtitleTask.loading)) return
        if (!Translator.textReady) {
            failedSubtitle = subtitleKey(track.url)
            subtitleTranslationFailed(Translator.hint)
            return
        }
        subtitleTranslationStarted = true
        translatingSubtitle = subtitleKey(track.url)
        subToastMsg = "Traduzione sottotitoli in corso…"
        subToastUntil = Date.now() + 8000
        subtitleTask = Translator.subtitles(track.url, currentUrl || url, subtitleLanguage(track.name), "it", {
            work: mediaTitle,
            chapter: episodeLine,
            scope: mediaTitle.length > 0 ? "anime:" + mediaTitle : ""
        })
    }

    function showTranslatedSubtitles(result) {
        if (!result) return
        translatedCues = Array.isArray(result.cues) ? result.cues : []
    }

    function removeTranslatedSubtitles() {
        translatedCues = []
        if (!web) return
        web.runJavaScript("(function(){try{var ov=document.getElementById('eno-sub-overlay');if(ov){ov.textContent='';ov.classList.remove('active');}var s=document.getElementById('eno-sub-style');if(s)s.remove();}catch(_){}})()")
    }

    function escapeRx(str) {
        return String(str).replace(/\\/g, "\\\\")
    }

    function resetSubtitles() {
        subtitleTracks = []
        subtitleSignature = ""
        currentSubtitleUrl = ""
        translatingSubtitle = ""
        failedSubtitle = ""
        subtitleTranslationStarted = false
        subtitleToastShown = false
        subToastUntil = 0
        subToastMsg = ""
        subtitleTask = null
        translatedCues = []
    }

    onVisibleChanged: {
        if (visible && translateSubs && subtitleTracks.length > 0 && translatedCues.length === 0)
            considerSubtitles(subtitleTracks, currentSubtitleUrl)
    }

    onTranslateSubsChanged: {
        subtitleSignature = ""
        failedSubtitle = ""
        if (translateSubs) {
            if (subtitleTracks.length > 0)
                considerSubtitles(subtitleTracks, currentSubtitleUrl)
            return
        }
        removeTranslatedSubtitles()
        translatingSubtitle = ""
        subtitleTranslationStarted = false
    }

    function wrapper(src) {
        const safe = String(src).replace(/"/g, "&quot;").replace(/</g, "&lt;")
        return "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            + "<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}iframe{border:0;width:100%;height:100%}</style>"
            + "</head><body><iframe id='f' src=\"" + safe + "\" allow='autoplay; fullscreen; encrypted-media' allowfullscreen referrerpolicy='origin'></iframe>"
            + "<script>"
            + "window.__sc={t:0,d:0,s:'',taps:0,fs:false};var f=document.getElementById('f');"
            + "function pub(){var v='sc:'+JSON.stringify(__sc);if(document.title!==v)document.title=v}"
            + "addEventListener('message',function(e){if(e.source!==f.contentWindow)return;var x=e.data;"
            + "if(typeof x==='string'){try{x=JSON.parse(x)}catch(_){return}}if(!x)return;"
            + "if(x.currentTime!==undefined){__sc.t=+x.currentTime||0;__sc.d=+x.duration||0}"
            + "if(x.playerStatus)__sc.s=String(x.playerStatus);"
            + "if(x.zenCommand==='toggleFullscreen'){__sc.fs=true;pub();}"
            + "if(x.zenCommand==='getFullscreenState'){f.contentWindow.postMessage({zenFullscreenState:false},'*');}"
            + "});"
            + "setInterval(function(){try{f.contentWindow.postMessage({command:'getTime'},'*')}catch(_){}},1000);"
            + "window.scSeek=function(v){f.contentWindow.postMessage({command:'seek',value:v},'*')};"
            + "addEventListener('blur',function(){__sc.taps++;pub();setTimeout(function(){try{document.activeElement.blur();window.focus()}catch(_){}},200)});"
            + "</script></body></html>"
    }

    // Desktop probe: monitors playback, manages subtitle & quality settings,
    // injects the smooth tendina overlay top bar & episode drawer, and handles actions.
    readonly property string probe: "(function(){"
        + "var w=window,isFs=" + (isFs ? "true" : "false") + ";"
        + "var chromeShown=" + (chromeShown ? "true" : "false") + ";"
        + "var resumeTarget=" + Number(resumeAt) + ";"
        + "var mediaTitle=" + JSON.stringify(mediaTitle) + ";"
        + "var episodeLine=" + JSON.stringify(episodeLine) + ";"
        + "var hasPrev=" + (hasPrev ? "true" : "false") + ";"
        + "var hasNext=" + (hasNext ? "true" : "false") + ";"
        + "var streams=" + streamsJson + ";"
        + "var curUrl=" + JSON.stringify(currentUrl) + ";"
        + "var activeSkip=" + activeSkipJson + ";"
        + "var episodes=" + episodesJson + ";"
        + "var currentIdx=" + Number(currentIndex) + ";"
        + "var subText=" + JSON.stringify(activeTranslatedText) + ";"
        + "var subTranslated=" + (translatedCues.length > 0 ? "true" : "false") + ";"
        + "var translateSubs=" + (translateSubs ? "true" : "false") + ";"
        + "var subBanner=" + JSON.stringify(Date.now() < subToastUntil ? subToastMsg : "") + ";"
        + "if(!w.__sc){"
        + "  w.__sc={k:[],toggleFs:false,subConfigured:false,subNotified:false,qualityConfigured:false,moved:false,act:null,drawerOpen:false,autoResumed:false,italianMissing:false,playTicks:0,subs:[],curSub:'',scanned:null};"
        + "  try{"
        + "    localStorage.setItem('zen-subtitle-quality','" + subQualityPref + "');"
        + "    localStorage.setItem('zen-quality-pref','1080p');"
        + "    localStorage.removeItem('artplayer-subtitle-pref-dub');"
        + "    if(translateSubs){localStorage.setItem('artplayer-subtitle-pref-sub','off');localStorage.setItem('artplayer-subtitle-pref-dub','off');}"
        + "    else if('" + subLangPref + "'==='it'){localStorage.setItem('artplayer-subtitle-pref-sub',JSON.stringify(['Italian']));}"
        + "  }catch(_){}"
        + "  try{if(w.Hls&&w.Hls.DefaultConfig){w.Hls.DefaultConfig.audioTrackLoadingTimeOut=30000;}}catch(_){}"
        + "  w.addEventListener('keydown',function(e){"
        + "    if(e.repeat||['Control','Shift','Alt','Meta'].indexOf(e.key)>=0)return;"
        + "    w.__sc.k.push({key:e.key,ctrl:e.ctrlKey,alt:e.altKey,shift:e.shiftKey,meta:e.metaKey});"
        + "  },true);"
        + "  w.addEventListener('pointermove',function(){w.__sc.moved=true;},true);"
        + "  w.addEventListener('message',function(e){"
        + "    if(e.data&&e.data.zenCommand==='toggleFullscreen'){w.__sc.toggleFs=true;}"
        + "    if(e.data&&e.data.zenCommand==='getFullscreenState'){w.postMessage({zenFullscreenState:isFs},'*');}"
        + "  });"
        + "  document.addEventListener('click',function(e){"
        + "    if(e.target&&e.target.closest&&e.target.closest('.art-control-fullscreen')){"
        + "      e.preventDefault();e.stopPropagation();w.__sc.toggleFs=true;"
        + "    }"
        + "  },true);"
        + "  document.addEventListener('dblclick',function(e){"
        + "    var p=e.target;"
        + "    if(p&&p.closest&&p.closest('.art-video-player')&&!p.closest('.art-bottom')&&!p.closest('#eno-topbar')&&!p.closest('#eno-drawer')){"
        + "      e.preventDefault();e.stopPropagation();w.__sc.toggleFs=true;"
        + "    }"
        + "  },true);"
        + "}"
        + "try{"
        + "  w.postMessage({zenFullscreenState:isFs},'*');"
        + "  var art=w.Artplayer&&w.Artplayer.instances&&w.Artplayer.instances[0];"
        + "  if(art){"
        + "    var maxRenderH='" + subQualityPref + "'==='low'?480:'" + subQualityPref + "'==='medium'?720:1080;"
        + "    if(art.plugins&&art.plugins.artplayerPluginLibass){"
        + "      if(art.plugins.artplayerPluginLibass.maxRenderHeight!==maxRenderH){"
        + "        art.plugins.artplayerPluginLibass.maxRenderHeight=maxRenderH;"
        + "        try{art.plugins.artplayerPluginLibass.resize();}catch(_){}"
        + "      }"
        + "    }"
        + "    var subOpt=null;"
        + "    var settingOpts=(art.setting&&art.setting.option)||(art.option&&art.option.settings)||null;"
        + "    if(settingOpts){"
        + "      subOpt=settingOpts.find(function(o){"
        + "        return o.html==='Subtitles'||o.name==='subtitles'||(o.selector&&o.selector.some(function(s){return s.name==='off';}));"
        + "      });"
        + "      if(!w.__sc.subConfigured&&subOpt&&subOpt.selector&&subOpt.selector.length>1){"
        + "        var matchSub=subOpt.selector.find(function(s){"
        + "          if(!s||s.name==='off')return false;"
        + "          var nm=String(s.name||s.html||'').toLowerCase();"
        + "          return '" + subLangPref + "'==='it'?(nm.indexOf('ita')>=0||nm.indexOf('italian')>=0):'" + subLangPref + "'==='en'?(nm.indexOf('eng')>=0||nm.indexOf('english')>=0):true;"
        + "        });"
        + "        if(matchSub){"
        + "          var curSubUrl=(art.subtitle&&art.subtitle.url)?art.subtitle.url:'';"
        + "          if(curSubUrl!==matchSub.url&&subOpt.onSelect){"
        + "            try{"
        + "              subOpt.onSelect(matchSub,null,{});"
        + "              localStorage.setItem('artplayer-subtitle-pref-sub',JSON.stringify([matchSub.name]));"
        + "              localStorage.removeItem('artplayer-subtitle-pref-dub');"
        + "            }catch(_){}"
        + "          }"
        + "          w.__sc.hasSubIta=true;"
        + "          w.__sc.subConfigured=true;"
        + "        }else{"
        + "          w.__sc.hasSubIta=false;"
        + "          if('" + subLangPref + "'==='it'&&!w.__sc.subNotified){"
        + "            w.__sc.italianMissing=true;"
        + "            w.__sc.subNotified=true;"
        + "          }"
        + "        }"
        + "      }"
        + "      if(subOpt&&subOpt.selector){"
        + "        var subs=[];"
        + "        for(var si=0;si<subOpt.selector.length;si++){"
        + "          var sub=subOpt.selector[si];"
        + "          if(sub&&sub.name!=='off'&&sub.url)subs.push({name:sub.name||sub.html||'',url:sub.url});"
        + "        }"
        + "        if(subs.length>0)w.__sc.subs=subs;"
        + "      }"
        + "    }"
        + "    if(art.subtitle&&art.subtitle.url){"
        + "      w.__sc.curSub=art.subtitle.url;"
        + "      if((!w.__sc.subs||w.__sc.subs.length===0)&&!w.__sc.scanned){"
        + "        w.__sc.subs=[{name:'Default',url:art.subtitle.url}];"
        + "      }"
        + "    }"
        + "    if(!w.__sc.subConfigured&&!w.__sc.subNotified&&art&&(art.playing||(art.video&&art.video.readyState>=2))){"
        + "      w.__sc.playTicks=(w.__sc.playTicks||0)+1;"
        + "      if(w.__sc.playTicks>=8&&'" + subLangPref + "'==='it'){"
        + "        w.__sc.italianMissing=true;"
        + "        w.__sc.subNotified=true;"
        + "      }"
        + "    }"
        + "    if(!w.__sc.qualityConfigured&&art.hls&&art.hls.levels&&art.hls.levels.length>0){"
        + "      var targetIdx=-1,targetH=0;"
        + "      for(var i=0;i<art.hls.levels.length;i++){"
        + "        var lvl=art.hls.levels[i];"
        + "        var h=lvl.height||0;"
        + "        if('" + videoQualityPref + "'==='max'){if(h>targetH){targetH=h;targetIdx=i;}}"
        + "        else if(h===720){targetH=h;targetIdx=i;break;}"
        + "      }"
        + "      if(targetIdx>=0&&art.hls.currentLevel!==targetIdx){art.hls.currentLevel=targetIdx;}"
        + "      if(art.setting&&art.setting.option){"
        + "        var qOpt=art.setting.option.find(function(o){return o.html==='Quality'||o.name==='hls-quality';});"
        + "        if(qOpt&&qOpt.selector&&qOpt.onSelect){"
        + "          var qItem=qOpt.selector.find(function(s){return(s.html&&s.html.indexOf(targetH+'p')>=0)||s.value===targetIdx;});"
        + "          if(qItem){try{qOpt.onSelect(qItem);}catch(_){}}"
        + "        }"
        + "      }"
        + "      w.__sc.qualityConfigured=true;"
        + "    }"
        + "    if(!w.__sc.hlsHooked&&art.hls){"
        + "      w.__sc.hlsHooked=true;"
        + "      try{"
        + "        if(art.hls.config){art.hls.config.audioTrackLoadingTimeOut=30000;}"
        + "        if(art.hls.on){"
        + "          art.hls.on('hlsError',function(ev,data){"
        + "            if(data&&(data.details==='audioTrackLoadTimeOut'||data.details==='audioTrackLoadError')){"
        + "              try{"
        + "                if(art.hls.audioTracks&&art.hls.audioTracks.length>0)art.hls.audioTrack=0;"
        + "                art.hls.recoverMediaError();"
        + "              }catch(_){}"
        + "            }"
        + "          });"
        + "        }"
        + "      }catch(_){}"
        + "    }"
        + "    if(!w.__sc.artErrHooked){"
        + "      w.__sc.artErrHooked=true;"
        + "      try{"
        + "        art.on('error',function(err){"
        + "          if(err&&String(err).toLowerCase().indexOf('audiotrack')>=0){"
        + "            try{if(art.hls){art.hls.audioTrack=0;art.hls.recoverMediaError();}}catch(_){}"
        + "          }"
        + "        });"
        + "      }catch(_){}"
        + "    }"
        + "    try{"
        + "      var notices=document.querySelectorAll('.art-notice,.art-error');"
        + "      for(var ni=0;ni<notices.length;ni++){"
        + "        if((notices[ni].textContent||'').toLowerCase().indexOf('audiotrack')>=0){"
        + "          notices[ni].style.display='none';"
        + "        }"
        + "      }"
        + "    }catch(_){}"
        + "  }"
        + "  if((!w.__sc.subs||w.__sc.subs.length===0)&&w.__sc.scanned)w.__sc.subs=w.__sc.scanned;"
        + "  if(!w.__sc.scanned&&document.scripts&&document.scripts.length>0){"
        + "    try{"
        + "      var scs=document.scripts,found=[];"
        + "      for(var sci=0;sci<scs.length;sci++){"
        + "        var tx=scs[sci].textContent||'';"
        + "        var at=tx.indexOf('subtitles:[{');"
        + "        if(at<0)continue;"
        + "        var seg=tx.slice(at,at+20000);"
        + "        var parts=seg.split('url:\"');"
        + "        for(var pi=1;pi<parts.length;pi++){"
        + "          var p=parts[pi];"
        + "          var uEnd=p.indexOf('\"');"
        + "          if(uEnd<0)continue;"
        + "          var u=p.slice(0,uEnd);"
        + "          var lIdx=p.indexOf('language:\"');"
        + "          if(lIdx<0||lIdx>400)continue;"
        + "          var lRest=p.slice(lIdx+10);"
        + "          var lEnd=lRest.indexOf('\"');"
        + "          if(lEnd<0)continue;"
        + "          found.push({name:lRest.slice(0,lEnd),url:u});"
        + "        }"
        + "        if(found.length)break;"
        + "      }"
        + "      if(found.length){w.__sc.subs=found;w.__sc.scanned=found;}"
        + "    }catch(_){}"
        + "  }"
        + "}catch(_){}"
        + "var v=document.querySelector('video');"
        + "if(!w.__sc.hookedSeek&&resumeTarget>5&&(art||v)){"
        + "  w.__sc.hookedSeek=true;"
        + "  var doSeek=function(){"
        + "    if(w.__sc.autoResumed)return;"
        + "    try{w.postMessage({command:'seek',value:resumeTarget},'*');}catch(_){}"
        + "    if(art&&art.duration>0){try{art.seek=resumeTarget;art.currentTime=resumeTarget;}catch(_){}"
        + "      if(art.video){try{art.video.currentTime=resumeTarget;}catch(_){}}"
        + "    }"
        + "    var vid=document.querySelector('video');"
        + "    if(vid&&vid.readyState>=1){try{vid.currentTime=resumeTarget;}catch(_){}}"
        + "  };"
        + "  if(art){try{art.on('video:loadedmetadata',doSeek);art.on('video:canplay',doSeek);art.on('ready',doSeek);art.on('video:play',doSeek);}catch(_){}}"
        + "  if(v){try{v.addEventListener('loadedmetadata',doSeek);v.addEventListener('canplay',doSeek);v.addEventListener('playing',doSeek);}catch(_){}}"
        + "  doSeek();"
        + "}"
        + "if(!w.__sc.autoResumed&&resumeTarget>5){"
        + "  if(v&&(v.readyState>=1||(art&&art.duration>0))){"
        + "    if(v.currentTime>=resumeTarget-4){"
        + "      w.__sc.autoResumed=true;"
        + "    }else if(v.currentTime>0||(art&&art.playing)){"
        + "      try{w.postMessage({command:'seek',value:resumeTarget},'*');}catch(_){}"
        + "      if(art&&art.duration>0){try{art.seek=resumeTarget;art.currentTime=resumeTarget;}catch(_){}"
        + "        if(art.video){try{art.video.currentTime=resumeTarget;}catch(_){}}"
        + "      }"
        + "      try{v.currentTime=resumeTarget;}catch(_){}"
        + "    }"
        + "  }"
        + "}"
        + "try{"
        + "  if(document.body){"
        + "    if(!document.getElementById('eno-styles')){"
        + "      var st=document.createElement('style');"
        + "      st.id='eno-styles';"
        + "      st.textContent='#eno-topbar{position:fixed;top:0;left:0;right:0;height:96px;background:linear-gradient(180deg,rgba(8,8,10,0.88) 0%,rgba(8,8,10,0.78) 22%,rgba(8,8,10,0.58) 46%,rgba(8,8,10,0.34) 68%,rgba(8,8,10,0.14) 84%,rgba(8,8,10,0) 100%);-webkit-backdrop-filter:blur(22px) saturate(145%);backdrop-filter:blur(22px) saturate(145%);-webkit-mask-image:linear-gradient(180deg,rgba(0,0,0,1) 0%,rgba(0,0,0,1) 48%,rgba(0,0,0,0.65) 72%,rgba(0,0,0,0.2) 88%,rgba(0,0,0,0) 100%);mask-image:linear-gradient(180deg,rgba(0,0,0,1) 0%,rgba(0,0,0,1) 48%,rgba(0,0,0,0.65) 72%,rgba(0,0,0,0.2) 88%,rgba(0,0,0,0) 100%);box-shadow:inset 0 1px 0 rgba(255,255,255,0.16);display:flex;align-items:flex-start;padding:14px 24px 0 24px;z-index:2147483647;transform:translateY(-100%);transition:transform 0.38s cubic-bezier(0.16,1,0.3,1),opacity 0.32s ease;opacity:0;pointer-events:none;box-sizing:border-box;font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;color:#fff;}'"
        + "        + '#eno-topbar.visible{transform:translateY(0);opacity:1;pointer-events:auto;}'"
        + "        + '.eno-btn-icon{width:38px;height:38px;border-radius:50%;border:1px solid transparent;background:transparent;color:#f4f4f5;display:flex;align-items:center;justify-content:center;cursor:pointer;transition:background 0.15s ease,color 0.15s ease,box-shadow 0.15s ease,border-color 0.15s ease;flex-shrink:0;}'"
        + "        + '.eno-btn-icon:hover{background:rgba(255,255,255,0.14);border-color:rgba(255,255,255,0.24);box-shadow:inset 0 1px 0 rgba(255,255,255,0.32),0 4px 12px rgba(0,0,0,0.28);-webkit-backdrop-filter:blur(16px) saturate(140%);backdrop-filter:blur(16px) saturate(140%);color:#fff;}'"
        + "        + '.eno-btn-icon:disabled{opacity:0.35;cursor:default;pointer-events:none;}'"
        + "        + '.eno-titles{display:flex;flex-direction:column;margin-left:12px;margin-top:2px;flex:1;min-width:0;}'"
        + "        + '.eno-media-title{font-size:15px;font-weight:600;color:#fff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;line-height:1.2;}'"
        + "        + '.eno-ep-line{font-size:12px;color:#a1a1aa;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:3px;}'"
        + "        + '.eno-skip-btn{background:#3b82f6;color:#fff;border:none;border-radius:8px;padding:6px 14px;font-size:13px;font-weight:600;cursor:pointer;margin-right:14px;margin-top:4px;display:inline-flex;align-items:center;gap:6px;flex-shrink:0;}'"
        + "        + '.eno-skip-btn:hover{background:#2563eb;}'"
        + "        + '.eno-servers{display:flex;align-items:center;gap:6px;margin-right:14px;margin-top:6px;flex-shrink:0;}'"
        + "        + '.eno-chip{background:rgba(255,255,255,0.10);-webkit-backdrop-filter:blur(16px) saturate(140%);backdrop-filter:blur(16px) saturate(140%);border:1px solid rgba(255,255,255,0.18);box-shadow:inset 0 1px 0 rgba(255,255,255,0.26);border-radius:9999px;color:#e4e4e7;padding:4px 12px;font-size:12px;font-weight:500;cursor:pointer;transition:all 0.15s;}'"
        + "        + '.eno-chip:hover{background:rgba(255,255,255,0.18);border-color:rgba(255,255,255,0.30);box-shadow:inset 0 1px 0 rgba(255,255,255,0.38),0 4px 12px rgba(0,0,0,0.24);color:#fff;}'"
        + "        + '.eno-chip.active{background:#fff;color:#0b0b0c;border-color:#fff;box-shadow:0 4px 14px rgba(0,0,0,0.32);font-weight:600;}'"
        + "        + '.eno-nav{display:flex;align-items:center;gap:4px;flex-shrink:0;}'"
        + "        + '#eno-drawer{position:fixed;top:12px;right:12px;bottom:12px;width:320px;border-radius:16px;overflow:hidden;background:rgba(14,14,18,0.68);-webkit-backdrop-filter:blur(28px) saturate(150%);backdrop-filter:blur(28px) saturate(150%);border:1px solid rgba(255,255,255,0.18);box-shadow:inset 0 1px 0 rgba(255,255,255,0.28),inset 0 0 18px rgba(255,255,255,0.05),-12px 12px 36px rgba(0,0,0,0.62);z-index:2147483646;transform:translateX(calc(100% + 24px));transition:transform 0.35s cubic-bezier(0.16,1,0.3,1);display:flex;flex-direction:column;font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;color:#fff;box-sizing:border-box;}'"
        + "        + '#eno-drawer.open{transform:translateX(0);}'"
        + "        + '.eno-dr-head{height:60px;display:flex;align-items:center;justify-content:space-between;padding:0 20px;border-bottom:1px solid rgba(255,255,255,0.08);font-size:16px;font-weight:600;}'"
        + "        + '.eno-dr-list{flex:1;overflow-y:auto;padding:8px;}'"
        + "        + '.eno-ep-item{display:flex;align-items:center;gap:10px;padding:10px 12px;border-radius:8px;cursor:pointer;margin-bottom:4px;transition:background 0.15s;}'"
        + "        + '.eno-ep-item:hover{background:rgba(255,255,255,0.08);}'"
        + "        + '.eno-ep-item.active{background:rgba(255,255,255,0.14);border-left:3px solid #3b82f6;}'"
        + "        + '.eno-ep-num{font-size:13px;font-weight:600;color:#fff;}'"
        + "        + '.eno-ep-name{font-size:11px;color:#a1a1aa;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:2px;}'"
        + "        + '#eno-sub-overlay{position:fixed;left:6%;right:6%;bottom:64px;z-index:2147483645;pointer-events:none;text-align:center;font-family:\"Gandhi Sans\",-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;font-weight:700;font-size:clamp(20px,2.6vw,30px);line-height:1.35;color:#ffffff;text-shadow:-2px -2px 0 #000,2px -2px 0 #000,-2px 2px 0 #000,2px 2px 0 #000,0 2px 8px rgba(0,0,0,0.92);white-space:pre-line;display:none;}'"
        + "        + '#eno-sub-overlay.active{display:block;}'"
        + "        + '#eno-sub-toast{position:fixed;top:88px;left:50%;transform:translateX(-50%);z-index:2147483647;pointer-events:none;background:rgba(16,185,129,0.92);-webkit-backdrop-filter:blur(16px);backdrop-filter:blur(16px);border:1px solid rgba(255,255,255,0.28);border-radius:999px;padding:7px 18px;font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;font-size:13px;font-weight:600;color:#fff;box-shadow:0 8px 24px rgba(0,0,0,0.45);opacity:0;transition:opacity 0.25s ease;}'"
        + "        + '#eno-sub-toast.show{opacity:1;}';"
        + "      document.head.appendChild(st);"
        + "    }"
        + "    if(!document.getElementById('eno-sub-toast')){"
        + "      var stb=document.createElement('div');"
        + "      stb.id='eno-sub-toast';"
        + "      stb.style.cssText='position:fixed;top:88px;left:50%;transform:translateX(-50%);z-index:2147483647;pointer-events:none;background:rgba(16,185,129,0.92);-webkit-backdrop-filter:blur(16px);backdrop-filter:blur(16px);border:1px solid rgba(255,255,255,0.28);border-radius:999px;padding:7px 18px;font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;font-size:13px;font-weight:600;color:#fff;box-shadow:0 8px 24px rgba(0,0,0,0.45);display:none;';"
        + "      document.body.appendChild(stb);"
        + "    }"
        + "    var stb=document.getElementById('eno-sub-toast');"
        + "    if(stb){"
        + "      if(subBanner.length>0){stb.textContent=subBanner;stb.style.display='block';}"
        + "      else{stb.style.display='none';}"
        + "    }"
        + "    if(!document.getElementById('eno-sub-overlay')){"
        + "      var sov=document.createElement('div');"
        + "      sov.id='eno-sub-overlay';"
        + "      sov.style.cssText='position:fixed;left:6%;right:6%;bottom:64px;z-index:2147483645;pointer-events:none;text-align:center;font-family:\"Gandhi Sans\",-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;font-weight:700;font-size:clamp(20px,2.6vw,30px);line-height:1.35;color:#ffffff;text-shadow:-2px -2px 0 #000,2px -2px 0 #000,-2px 2px 0 #000,2px 2px 0 #000,0 2px 8px rgba(0,0,0,0.92);white-space:pre-line;display:none;';"
        + "      document.body.appendChild(sov);"
        + "    }"
        + "    var sov=document.getElementById('eno-sub-overlay');"
        + "    if(sov){"
        + "      if(sov.textContent!==subText)sov.textContent=subText;"
        + "      sov.style.display=subText.length>0?'block':'none';"
        + "    }"
        + "    if(subTranslated||translateSubs){"
        + "      if(!document.getElementById('eno-sub-style')){"
        + "        var sst=document.createElement('style');"
        + "        sst.id='eno-sub-style';"
        + "        sst.textContent='.art-subtitle,.artplayer-plugin-libass,.libassjs-canvas-parent,.libassjs-canvas,.art-layer-subtitleLoading,.subtitle-toast{display:none!important;opacity:0!important;visibility:hidden!important}';"
        + "        document.head.appendChild(sst);"
        + "      }"
        + "      if(art&&art.subtitle)art.subtitle.show=false;"
        + "      if(art&&art.plugins&&art.plugins.artplayerPluginLibass){"
        + "        try{art.plugins.artplayerPluginLibass.hide();}catch(_){}"
        + "        try{if(art.plugins.artplayerPluginLibass.libass)art.plugins.artplayerPluginLibass.libass.freeTrack();}catch(_){}"
        + "      }"
        + "    }"
        + "    if(!document.getElementById('eno-topbar')){"
        + "      var tb=document.createElement('div');"
        + "      tb.id='eno-topbar';"
        + "      tb.innerHTML='<button class=\"eno-btn-icon\" id=\"eno-btn-back\" title=\"Indietro\"><svg width=\"22\" height=\"22\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2.2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"m12 19-7-7 7-7\"/><path d=\"M19 12H5\"/></svg></button>'"
        + "        + '<div class=\"eno-titles\"><div class=\"eno-media-title\" id=\"eno-txt-title\"></div><div class=\"eno-ep-line\" id=\"eno-txt-ep\"></div></div>'"
        + "        + '<div id=\"eno-center-box\"></div>'"
        + "        + '<div class=\"eno-servers\" id=\"eno-servers-box\"></div>'"
        + "        + '<div class=\"eno-nav\">'"
        + "        + '<button class=\"eno-btn-icon\" id=\"eno-btn-prev\" title=\"Episodio precedente\"><svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"19 20 9 12 19 4 19 20\"/><line x1=\"5\" x2=\"5\" y1=\"19\" y2=\"5\"/></svg></button>'"
        + "        + '<button class=\"eno-btn-icon\" id=\"eno-btn-next\" title=\"Episodio successivo\"><svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 4 15 12 5 20 5 4\"/><line x1=\"19\" x2=\"19\" y1=\"5\" y2=\"19\"/></svg></button>'"
        + "        + '<button class=\"eno-btn-icon\" id=\"eno-btn-drawer\" title=\"Episodi\"><svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"8\" x2=\"21\" y1=\"6\" y2=\"6\"/><line x1=\"8\" x2=\"21\" y1=\"12\" y2=\"12\"/><line x1=\"8\" x2=\"21\" y1=\"18\" y2=\"18\"/><line x1=\"3\" x2=\"3.01\" y1=\"6\" y2=\"6\"/><line x1=\"3\" x2=\"3.01\" y1=\"12\" y2=\"12\"/><line x1=\"3\" x2=\"3.01\" y1=\"18\" y2=\"18\"/></svg></button>'"
        + "        + '</div>';"
        + "      document.body.appendChild(tb);"
        + "      document.getElementById('eno-btn-back').onclick=function(e){e.stopPropagation();w.__sc.act={name:'back'};};"
        + "      document.getElementById('eno-btn-prev').onclick=function(e){e.stopPropagation();w.__sc.act={name:'prev'};};"
        + "      document.getElementById('eno-btn-next').onclick=function(e){e.stopPropagation();w.__sc.act={name:'next'};};"
        + "      document.getElementById('eno-btn-drawer').onclick=function(e){"
        + "        e.stopPropagation();w.__sc.drawerOpen=!w.__sc.drawerOpen;"
        + "        var dr=document.getElementById('eno-drawer');if(dr)dr.classList.toggle('open',w.__sc.drawerOpen);"
        + "      };"
        + "      tb.addEventListener('pointerenter',function(){w.__sc.moved=true;},true);"
        + "    }"
        + "    if(!document.getElementById('eno-drawer')){"
        + "      var dr=document.createElement('div');"
        + "      dr.id='eno-drawer';"
        + "      dr.innerHTML='<div class=\"eno-dr-head\"><span>Episodi</span><button class=\"eno-btn-icon\" id=\"eno-dr-close\"><svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"6\" x2=\"6\" y2=\"18\"></line><line x1=\"6\" y1=\"6\" x2=\"18\" y2=\"18\"></line></svg></button></div><div class=\"eno-dr-list\" id=\"eno-dr-list\"></div>';"
        + "      document.body.appendChild(dr);"
        + "      document.getElementById('eno-dr-close').onclick=function(e){"
        + "        e.stopPropagation();w.__sc.drawerOpen=false;dr.classList.remove('open');"
        + "      };"
        + "      dr.addEventListener('pointerenter',function(){w.__sc.moved=true;},true);"
        + "    }"
        + "    var tb=document.getElementById('eno-topbar');"
        + "    if(tb){"
        + "      if(chromeShown)tb.classList.add('visible');else tb.classList.remove('visible');"
        + "      var tEl=document.getElementById('eno-txt-title');if(tEl&&tEl.textContent!==mediaTitle)tEl.textContent=mediaTitle;"
        + "      var epEl=document.getElementById('eno-txt-ep');if(epEl&&epEl.textContent!==episodeLine)epEl.textContent=episodeLine;"
        + "      var pBtn=document.getElementById('eno-btn-prev');if(pBtn)pBtn.disabled=!hasPrev;"
        + "      var nBtn=document.getElementById('eno-btn-next');if(nBtn)nBtn.disabled=!hasNext;"
        + "      var cBox=document.getElementById('eno-center-box');"
        + "      if(cBox){"
        + "        if(activeSkip){"
        + "          if(!cBox.firstChild||cBox.dataset.skip!==String(activeSkip.start)){"
        + "            cBox.dataset.skip=String(activeSkip.start);"
        + "            var lbl=activeSkip.type&&activeSkip.type.indexOf('ed')>=0?'Salta ending':'Salta intro';"
        + "            cBox.innerHTML='<button class=\"eno-skip-btn\"><svg width=\"14\" height=\"14\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 4 15 12 5 20 5 4\"/><line x1=\"19\" x2=\"19\" y1=\"5\" y2=\"19\"/></svg>'+lbl+'</button>';"
        + "            cBox.firstChild.onclick=function(e){e.stopPropagation();w.__sc.act={name:'skip',to:activeSkip.end};};"
        + "          }"
        + "        }else{"
        + "          cBox.innerHTML='';"
        + "          delete cBox.dataset.skip;"
        + "        }"
        + "      }"
        + "      var sBox=document.getElementById('eno-servers-box');"
        + "      if(sBox&&streams&&streams.length>0){"
        + "        var curSig=streams.map(function(s){return s.quality;}).join(',')+'|'+curUrl;"
        + "        if(sBox.dataset.sig!==curSig){"
        + "          sBox.dataset.sig=curSig;"
        + "          sBox.innerHTML='';"
        + "          streams.forEach(function(s){"
        + "            var chip=document.createElement('button');"
        + "            chip.className='eno-chip'+(s.url===curUrl?' active':'');"
        + "            chip.textContent=s.quality;"
        + "            chip.onclick=function(e){e.stopPropagation();w.__sc.act={name:'server',url:s.url,quality:s.quality};};"
        + "            sBox.appendChild(chip);"
        + "          });"
        + "        }"
        + "      }"
        + "      var dr=document.getElementById('eno-drawer');"
        + "      if(dr){"
        + "        if(w.__sc.drawerOpen)dr.classList.add('open');else dr.classList.remove('open');"
        + "        var dList=document.getElementById('eno-dr-list');"
        + "        if(dList&&episodes&&episodes.length>0){"
        + "          var epSig=episodes.length+'|'+currentIdx;"
        + "          if(dList.dataset.sig!==epSig){"
        + "            dList.dataset.sig=epSig;"
        + "            dList.innerHTML='';"
        + "            episodes.forEach(function(ep){"
        + "              var item=document.createElement('div');"
        + "              item.className='eno-ep-item'+(ep.i===currentIdx?' active':'');"
        + "              item.innerHTML='<div class=\"eno-ep-num\">Episodio '+ep.n+'</div><div class=\"eno-ep-name\">'+(ep.t||'')+'</div>';"
        + "              item.onclick=function(e){"
        + "                e.stopPropagation();w.__sc.act={name:'episode',index:ep.i};"
        + "                w.__sc.drawerOpen=false;dr.classList.remove('open');"
        + "              };"
        + "              dList.appendChild(item);"
        + "            });"
        + "          }"
        + "        }"
        + "      }"
        + "    }"
        + "  }"
        + "}catch(_){}"
        + "var k=w.__sc.k;w.__sc.k=[];"
        + "try{if(w.__sc&&w.__sc.enoSyncSubs)w.__sc.enoSyncSubs();}catch(_){}"
        + "var toggleFs=w.__sc.toggleFs;w.__sc.toggleFs=false;"
        + "var moved=w.__sc.moved;w.__sc.moved=false;"
        + "var act=w.__sc.act;w.__sc.act=null;"
        + "var noIta=!!w.__sc.italianMissing;w.__sc.italianMissing=false;"
        + "return JSON.stringify({t:v?v.currentTime:0,d:v&&isFinite(v.duration)?v.duration:0,s:v&&v.ended?'Ended':'',k:k,fs:toggleFs,mv:moved,act:act,noIta:noIta,subs:w.__sc.subs||[],curSub:w.__sc.curSub||''});"
        + "})()"

    function host(address) {
        return String(address).replace(/^[a-z]+:\/\//i, "").replace(/[\/?#].*$/, "").toLowerCase()
    }

    // Phones: video with sound, and the player's own full screen. The page
    // loads once the web view is prepared for them.
    property bool ready: direct
    Component.onCompleted: if (!direct) App.prepareWebPlayer()
    Connections {
        target: App
        enabled: !root.ready
        function onWebPlayerPrepared() { root.ready = true }
    }

    function load() {
        if (!url || !ready) return
        if (direct) web.url = url
        else web.loadHtml(wrapper(url), "https://reanime.to/")
    }
    onReadyChanged: load()
    onUrlChanged: {
        resetSubtitles()
        load()
    }

    Connections {
        target: root.Window.window
        function onVisibilityChanged() {
            root.syncFullscreen()
        }
    }

    WebView {
        id: web
        anchors.fill: parent
        onTitleChanged: {
            if (root.direct) return
            const title = String(web.title)
            if (!title.startsWith("sc:")) return
            try {
                const s = JSON.parse(title.slice(3))
                if (s.taps > root.taps) {
                    root.taps = s.taps
                    root.interacted()
                }
                if (s.fs) {
                    root.toggleFullScreen()
                }
                if (s.d > 0) root.time(s.t, s.d, s.s || "")
            } catch (e) {}
        }
        // Desktop: the player page stays put; an ad that navigates the page
        // away is sent back. (Pop-up windows are refused by the web view.)
        onLoadingChanged: (request) => {
            if (root.direct && root.url && request.status === WebView.LoadStartedStatus
                && root.host(request.url) !== root.host(root.url))
                web.url = root.url
            if (root.direct && request.status === WebView.LoadStartedStatus) {
                web.runJavaScript("(function(){try{localStorage.removeItem('artplayer-subtitle-pref-dub');"
                    + (root.translateSubs ? "localStorage.setItem('artplayer-subtitle-pref-sub','off');localStorage.setItem('artplayer-subtitle-pref-dub','off');"
                       : (root.subLangPref === 'it' ? "localStorage.setItem('artplayer-subtitle-pref-sub',JSON.stringify(['Italian']));" : ""))
                    + "if(window.Hls&&window.Hls.DefaultConfig){window.Hls.DefaultConfig.audioTrackLoadingTimeOut=30000;}}catch(_){}})()")
            }
            if (root.direct && request.status === WebView.LoadSucceededStatus) {
                const subQuality = root.subQualityPref
                const initJs = "(function(){try{"
                    + "localStorage.setItem('zen-subtitle-quality','" + subQuality + "');"
                    + "localStorage.setItem('zen-quality-pref','1080p');"
                    + "localStorage.removeItem('artplayer-subtitle-pref-dub');"
                    + (root.translateSubs ? "localStorage.setItem('artplayer-subtitle-pref-sub','off');localStorage.setItem('artplayer-subtitle-pref-dub','off');"
                       : (root.subLangPref === 'it' ? "localStorage.setItem('artplayer-subtitle-pref-sub',JSON.stringify(['Italian']));" : ""))
                    + (root.resumeAt > 5 ? "sessionStorage.setItem('zen-ios-resume','" + Number(root.resumeAt) + "');" : "")
                    + "if(window.Hls&&window.Hls.DefaultConfig){window.Hls.DefaultConfig.audioTrackLoadingTimeOut=30000;}"
                    + "}catch(_){}})()"
                web.runJavaScript(initJs)
                root.syncFullscreen()
            }
        }
    }

    Connections {
        target: root.subtitleTask
        function onProgressed(partial) {
            if (!partial || !Array.isArray(partial.cues)) return
            root.showTranslatedSubtitles(partial)
            if (!root.subtitleToastShown) {
                root.subtitleToastShown = true
                root.subToastMsg = "Sottotitoli tradotti in italiano"
                root.subToastUntil = Date.now() + 3500
                root.subtitleTranslated(Qt.locale("it").nativeLanguageName || "it")
            }
        }
        function onFinished(result) {
            if (!result || !Array.isArray(result.cues)) return
            root.showTranslatedSubtitles(result)
            if (!root.subtitleToastShown) {
                root.subtitleToastShown = true
                root.subToastMsg = "Sottotitoli tradotti in italiano"
                root.subToastUntil = Date.now() + 3500
                root.subtitleTranslated(Qt.locale("it").nativeLanguageName || "it")
            }
        }
        function onFailed(error) {
            root.failedSubtitle = root.translatingSubtitle
            root.subtitleTranslationStarted = false
            root.subToastMsg = String(error || "Errore traduzione sottotitoli")
            root.subToastUntil = Date.now() + 4000
            root.subtitleTranslationFailed(error)
            if (App.settings.autoItalianFallback !== false) root.italianMissing()
        }
    }

    Timer {
        interval: 200
        running: root.direct && root.visible && root.url.length > 0
        repeat: true
        onTriggered: web.runJavaScript(root.probe, (result) => {
            try {
                const s = JSON.parse(result)
                root.videoTimeMs = (Number(s.t) || 0) * 1000
                for (const k of s.k || []) root.key(k)
                if (s.fs) root.toggleFullScreen()
                if (s.mv) root.interacted()
                if (s.act) {
                    if (s.act.name === 'back') root.backClicked()
                    else if (s.act.name === 'prev') root.prevClicked()
                    else if (s.act.name === 'next') root.nextClicked()
                    else if (s.act.name === 'server') root.serverPicked(s.act.url, s.act.quality)
                    else if (s.act.name === 'skip') root.skipClicked(s.act.to)
                    else if (s.act.name === 'episode') root.episodeSelected(s.act.index)
                }
                if (Array.isArray(s.subs)) root.considerSubtitles(s.subs, s.curSub || "")
                if (s.noIta && App.settings.autoItalianFallback !== false && !root.subtitleTranslationStarted) root.italianMissing()
                if (s.d > 0) root.time(s.t, s.d, s.s || "")
            } catch (e) {}
        })
    }
}
