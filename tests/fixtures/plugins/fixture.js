// A tiny LNReader-style plugin for SCANCORE's tests (written for them, not
// taken from any repository). Shaped like the compiled plugins: CommonJS,
// require() for its modules, promises, and exports.default an instance.
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var fetch_1 = require("@libs/fetch");
var novelStatus_1 = require("@libs/novelStatus");
var storage_1 = require("@libs/storage");
var filterInputs_1 = require("@libs/filterInputs");
var cheerio_1 = require("cheerio");
var htmlparser2_1 = require("htmlparser2");
var dayjs_1 = require("dayjs");

function wait(ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); }

var FixturePlugin = (function () {
    function FixturePlugin() {
        this.id = "fixture";
        this.name = "Fixture Novels";
        this.version = "1.2.0";
        this.site = "https://novels.test/";
        this.icon = "src/en/fixture/icon.png";
        this.filters = {
            sort: { type: filterInputs_1.FilterTypes.Picker, label: "Sort", value: "new",
                    options: [{ label: "Newest", value: "new" }, { label: "Popular", value: "hot" }] }
        };
        this.pluginSettings = { greeting: { value: "", label: "Greeting", type: "Text" } };
    }
    FixturePlugin.prototype.popularNovels = function (page, options) {
        var params = new URLSearchParams({ page: page, sort: options && options.filters ? options.filters.sort.value : "new" });
        var url = new URL("popular?" + params.toString(), this.site);
        return wait(5).then(function () { return fetch_1.fetchApi(url.toString()); })
            .then(function (r) { return r.text(); })
            .then(function (html) {
                var novels = [];
                var re = /<a href="([^"]+)" data-cover="([^"]*)">([^<]+)<\/a>/g, m;
                while ((m = re.exec(html))) novels.push({ name: m[3], path: m[1], cover: m[2] });
                return novels;
            });
    };
    FixturePlugin.prototype.searchNovels = function (term, page) {
        var body = new FormData();
        body.append("q", term);
        body.append("page", String(page));
        return fetch_1.fetchApi(this.site + "search", { method: "POST", body: body })
            .then(function (r) { return r.json(); })
            .then(function (json) { return json.results; });
    };
    FixturePlugin.prototype.parseNovel = function (path) {
        var site = this.site;
        return fetch_1.fetchText(site + path.replace(/^\//, "")).then(function (html) {
            // Flag "s": QJSEngine does not know it; SCANCORE rewrites it.
            var summary = /<div class="summary">(.*?)<\/div>/s.exec(html);
            var chapters = [];
            var re = /<li data-path="([^"]+)" data-time="([^"]*)">([^<]+)<\/li>/g, m;
            while ((m = re.exec(html))) chapters.push({ name: m[3], path: m[1], releaseTime: m[2] });
            return {
                path: path,
                name: /<h1>([^<]+)<\/h1>/.exec(html)[1],
                author: "A. Writer",
                status: novelStatus_1.NovelStatus.Ongoing,
                genres: "Fantasy, Drama",
                summary: summary ? summary[1].trim() : "",
                chapters: chapters
            };
        });
    };
    FixturePlugin.prototype.parseChapter = function (path) {
        return fetch_1.fetchText(this.site + path.replace(/^\//, ""));
    };
    // Test helpers.
    FixturePlugin.prototype.remember = function (value) {
        storage_1.storage.set("last", { value: value });
        return Promise.resolve(storage_1.storage.get("last").value);
    };
    FixturePlugin.prototype.recall = function () { return storage_1.storage.get("last") || null; };
    FixturePlugin.prototype.spin = function () { for (;;) {} };
    FixturePlugin.prototype.local = function () { return fetch_1.fetchApi("http://127.0.0.1:9/").then(function () { return "reached"; }); };
    FixturePlugin.prototype.reach = function (url) { return fetch_1.fetchApi(url).then(function () { return "reached"; }); };
    FixturePlugin.prototype.fails = function () { return Promise.reject(new Error("The site changed.")); };
    // The bundled libraries, as plugins use them.
    FixturePlugin.prototype.scrape = function (html) {
        var $ = (0, cheerio_1.load)(html);
        $(".ad").remove();
        var titles = $("ul.list > li a").map(function (i, el) { return $(el).text().trim(); }).get();
        var links = $("ul.list a").first().attr("href");
        var count = 0;
        var parser = new htmlparser2_1.Parser({ onopentag: function (name) { if (name === "li") count++; } });
        parser.write(html);
        parser.end();
        var date = dayjs_1("2026-01-15T10:00:00Z").subtract(2, "day").format("YYYY-MM-DD");
        return { titles: titles, first: links, items: count, date: date, body: $("p.body").html() };
    };
    FixturePlugin.prototype.text = function () {
        var bytes = new TextEncoder().encode("héllo");
        return [bytes.length, new TextDecoder().decode(bytes), btoa("hi"), atob("aGk="), new URL("/a?b=1#c", "https://x.test/p/").href];
    };
    return FixturePlugin;
}());
exports.default = new FixturePlugin();
