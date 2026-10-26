# Music extensions

SCANCORE's Music section plays anime songs (AnimeThemes), your own files and
songs from **music extensions**: small JavaScript plugins that anyone can
write and publish in their own repository. SCANCORE doesn't list or host any
repositories itself; you add the ones you trust in Extensions → Add
repository.

This page is the contract (version 1). It is MIT licensed, like SCANCORE;
your plugin can use whatever licence you like.

## The plugin file

A plugin is one CommonJS JavaScript file whose `exports.default` is an object
(or a class instance), the same shape as an LNReader plugin. It runs in the
same runtime as LNReader plugins:

- `require("@libs/fetch").fetchApi(url, init)` for requests (SCANCORE's
  network, cookies and user agent), plus `fetch`, `URL`, `URLSearchParams`,
  `setTimeout` and `atob`/`btoa`.
- `require("cheerio")`, `require("htmlparser2")`, `require("dayjs")`.
- `require("@libs/storage").storage` to keep small values between calls.

Every method may return a promise. A call that runs longer than 30 seconds
is stopped.

```js
"use strict";
var fetchApi = require("@libs/fetch").fetchApi;

exports.default = {
    id: "example",                // unique, letters, digits, "." "_" "-"
    name: "Example Music",
    version: "1.0.0",
    site: "https://music.example/",
    lang: "en",

    // Required: songs for a search. `page` starts at 1.
    searchSongs: function (query, page) {
        return fetchApi("https://music.example/api/search?q=" + encodeURIComponent(query) + "&page=" + page)
            .then(function (r) { return r.json(); })
            .then(function (data) {
                return data.tracks.map(function (t) {
                    return { id: t.id, title: t.title, artists: [t.artist], album: t.album,
                             cover: t.image, duration: t.seconds, slug: t.slug };
                });
            });
    },

    // Required: where a song plays. A URL, or {url, headers}.
    streamOf: function (song) {
        return { url: "https://music.example/stream/" + song.slug, headers: { Referer: "https://music.example/" } };
    },

    // Optional: songs to show before any search.
    popularSongs: function (page) { return []; },

    // Optional: lyrics. `synced` is LRC text ("[01:02.50] words").
    lyricsOf: function (song) { return { synced: "", plain: "" }; }
};
```

### Songs

| Field | | |
|---|---|---|
| `id` | required | Text or a number, unique within the plugin. |
| `title` | required | |
| `artists` | | An array of names (or `artist`, one name). |
| `album` | | |
| `cover` | | An https image. |
| `duration` | | Seconds; helps find lyrics. |

Anything else you put on a song (a slug, a token) comes back to
`streamOf` and `lyricsOf` exactly as you gave it.

Streams must be `https` (or `http`) addresses that mpv can play: MP3, AAC,
Ogg/Opus, FLAC, HLS and so on.

### Lyrics

When a song plays, SCANCORE asks `lyricsOf` first (when there is one), then
LRCLIB by title and artist.

## The repository

A repository is one JSON file served over https:

```json
{
  "name": "My music extensions",
  "music": [
    {
      "id": "example",
      "name": "Example Music",
      "version": "1.0.0",
      "lang": "en",
      "site": "https://music.example/",
      "url": "example.js",
      "iconUrl": "example.png"
    }
  ]
}
```

`url` and `iconUrl` may be relative to the repository file. Raise `version`
for an update; SCANCORE offers it from Extensions → Updates.

People add it from Extensions → Add repository, or with a link:
`scancore://repos/add?url=https://you.example/music.json`.

## Rules

Extensions are added by the user, from repositories the user chose.
SCANCORE keeps a kill-switch for the extension system. Only publish plugins
for music you are allowed to share.
