pragma Singleton
import QtQuick

// The small page web players load around a provider's player (an iframe):
// it reads the player's time through getTime/seek messages, and on phones
// lays a see-through shield over it, so taps reach SCANCORE and never the
// embedded site (whose ads redirect the player):
//   - before playback starts the shield lets taps through (the site's own
//     play button), and arms itself once the time moves;
//   - a tap shows SCANCORE's controls; a double tap on the left or right
//     third seeks 10 s, in the middle it pauses or plays;
//   - a long press lets taps through for 8 s (the site's menus);
//   - if the player's frame is navigated away after playback started (an
//     ad), it is put back and seeks to where it was.
// Reports go to window.__sc, and with `titled` also to the document title
// (Android's web view, see WebPlayerSystem).
QtObject {
    function wrapper(src, shield, titled) {
        const safe = String(src).replace(/"/g, "&quot;").replace(/</g, "&lt;")
        return "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            + "<style>html,body{margin:0;height:100%;background:#000;overflow:hidden;-webkit-user-select:none;user-select:none}"
            + "iframe{border:0;width:100%;height:100%}"
            + "#sh{position:fixed;inset:0;z-index:2;pointer-events:none;-webkit-tap-highlight-color:transparent;touch-action:none}"
            + "#hint{position:fixed;left:50%;top:14px;transform:translateX(-50%);z-index:3;padding:6px 12px;border-radius:14px;"
            + "background:rgba(0,0,0,.7);color:#fff;font:13px sans-serif;opacity:0;transition:opacity .2s;pointer-events:none}</style>"
            + "</head><body><iframe id='f' src=\"" + safe + "\" allow='autoplay; fullscreen; encrypted-media; picture-in-picture' allowfullscreen referrerpolicy='origin'></iframe>"
            + "<div id='sh'></div><div id='hint'></div>"
            + "<script>(function(){"
            + "var S=window.__sc={t:0,d:0,s:'',taps:0,armed:false,h264:!!(window.MediaSource&&MediaSource.isTypeSupported('video/mp4; codecs=\"avc1.42E01E\"'))};"
            + "var f=document.getElementById('f'),sh=document.getElementById('sh'),hint=document.getElementById('hint'),src=f.src;"
            + "var shield=" + (shield ? "true" : "false") + ",titled=" + (titled ? "true" : "false") + ",last=-1,loads=0,restoring=false,paused=false;"
            + "function pub(){if(!titled)return;var v='sc:'+JSON.stringify(S);if(document.title!==v)document.title=v}"
            + "function post(m){try{f.contentWindow.postMessage(m,'*')}catch(_){}}"
            + "function say(t){hint.textContent=t;hint.style.opacity=1;clearTimeout(say.t);say.t=setTimeout(function(){hint.style.opacity=0},1400)}"
            + "function arm(){if(!shield||S.armed)return;S.armed=true;sh.style.pointerEvents='auto'}"
            + "function open(ms){S.armed=false;sh.style.pointerEvents='none';setTimeout(arm,ms)}"
            + "addEventListener('message',function(e){if(e.source!==f.contentWindow)return;var x=e.data;"
            + "if(typeof x==='string'){try{x=JSON.parse(x)}catch(_){return}}if(!x)return;"
            + "if(x.currentTime!==undefined){var t=+x.currentTime||0;if(last>=0&&t>last)arm();last=t;S.t=t;S.d=+x.duration||0}"
            + "if(x.playerStatus)S.s=String(x.playerStatus);pub()});"
            + "setInterval(function(){post({command:'getTime'})},1000);"
            + "window.scSeek=function(v){post({command:'seek',value:v})};"
            // The frame went somewhere else after playback started: back.
            + "f.addEventListener('load',function(){loads++;if(restoring){restoring=false;return}"
            + "if(loads>1&&S.armed){var at=S.t;restoring=true;f.src=src;setTimeout(function(){window.scSeek(at)},2500)}});"
            + "var tap=0,one=null,press=null;"
            + "sh.addEventListener('pointerdown',function(){clearTimeout(press);press=setTimeout(function(){press=null;open(8000);say('Player controls for 8 s')},600)});"
            + "sh.addEventListener('pointerup',function(e){if(!press)return;clearTimeout(press);press=null;var now=Date.now(),w=innerWidth,x=e.clientX;"
            + "if(now-tap<300){clearTimeout(one);tap=0;"
            + "if(x<w/3){window.scSeek(Math.max(0,S.t-10));say('−10 s')}else if(x>2*w/3){window.scSeek(S.t+10);say('+10 s')}"
            + "else{paused=!paused;post({command:paused?'pause':'play'});post(paused?'pause':'play');say(paused?'Paused':'Playing')}}"
            + "else{tap=now;one=setTimeout(function(){S.taps++;pub()},300)}});"
            + "sh.addEventListener('pointercancel',function(){clearTimeout(press);press=null});"
            // Without the shield, a tap into the frame takes the page's focus:
            // counted, and handed back for the next tap.
            + "addEventListener('blur',function(){S.taps++;pub();setTimeout(function(){try{document.activeElement.blur();window.focus()}catch(_){}},200)});"
            + "})()</script></body></html>"
    }
}
