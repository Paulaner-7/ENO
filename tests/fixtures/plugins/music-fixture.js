// A tiny music plugin for SCANCORE's tests (docs/music-plugins.md), against
// a fixture site on music.test.
"use strict";
var fetchApi = require("@libs/fetch").fetchApi;

exports.default = {
    id: "musicfix",
    name: "Fixture Music",
    version: "1.0.0",
    site: "https://music.test/",
    lang: "en",
    searchSongs: function (query, page) {
        return fetchApi("https://music.test/search?q=" + encodeURIComponent(query) + "&page=" + page)
            .then(function (r) { return r.json(); })
            .then(function (data) {
                return data.map(function (t) {
                    return { id: t.n, title: t.name, artist: t.by, duration: t.s, slug: "s" + t.n };
                });
            });
    },
    streamOf: function (song) {
        return { url: "https://music.test/stream/" + song.slug + ".ogg", headers: { Referer: "https://music.test/" } };
    },
    lyricsOf: function (song) {
        return Promise.resolve({ synced: "[00:01.00]one\n[00:02.00]two" });
    }
};
