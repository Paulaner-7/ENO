// SCANCORE's side of the LNReader plugin API, evaluated once in every
// plugin's engine (src/plugins/Runtime.cpp) before the plugin itself.
// `__native` is the C++ host (network, storage, timers, text encoding);
// `__vendor` holds the bundled libraries when they are present
// (resources/plugins/vendor.js: cheerio, htmlparser2, dayjs, ...).
//
// It provides what LNReader gives its plugins: the modules they `require`
// and the web APIs the compiled bundles use (setTimeout, URL,
// URLSearchParams, AbortController, FormData, Headers, TextEncoder and
// TextDecoder, atob and btoa).

(function (global) {
    "use strict";

    // ------------------------------------------------------------ timers
    var timers = {};
    global.setTimeout = function (fn, ms) {
        var args = Array.prototype.slice.call(arguments, 2);
        var id = __native.setTimeout(Math.max(0, Number(ms) || 0));
        timers[id] = function () { fn.apply(null, args); };
        return id;
    };
    global.clearTimeout = function (id) {
        if (timers[id]) { delete timers[id]; __native.clearTimeout(id); }
    };
    global.setInterval = function (fn, ms) {
        var holder = {};
        var tick = function () { holder.id = global.setTimeout(tick, ms); fn(); };
        holder.id = global.setTimeout(tick, ms);
        return holder;
    };
    global.clearInterval = function (holder) { if (holder) global.clearTimeout(holder.id); };
    global.__fireTimer = function (id) {
        var fn = timers[id];
        delete timers[id];
        if (fn) fn();
    };

    // ------------------------------------------------------------ URLSearchParams
    function decodePart(s) {
        try { return decodeURIComponent(s.replace(/\+/g, " ")); } catch (e) { return s; }
    }
    function encodePart(s) {
        return encodeURIComponent(s).replace(/%20/g, "+").replace(/[!'()~]/g, function (c) {
            return "%" + c.charCodeAt(0).toString(16).toUpperCase();
        });
    }
    function URLSearchParams(init) {
        this._list = [];
        if (init === undefined || init === null) return;
        if (typeof init === "string") {
            var s = init.charAt(0) === "?" ? init.slice(1) : init;
            var parts = s.split("&");
            for (var i = 0; i < parts.length; ++i) {
                if (!parts[i]) continue;
                var eq = parts[i].indexOf("=");
                this._list.push(eq < 0 ? [decodePart(parts[i]), ""]
                                       : [decodePart(parts[i].slice(0, eq)), decodePart(parts[i].slice(eq + 1))]);
            }
        } else if (init instanceof URLSearchParams) {
            this._list = init._list.slice();
        } else if (Array.isArray(init)) {
            for (var j = 0; j < init.length; ++j) this._list.push([String(init[j][0]), String(init[j][1])]);
        } else if (typeof init === "object") {
            for (var k in init) if (Object.prototype.hasOwnProperty.call(init, k)) this._list.push([k, String(init[k])]);
        }
    }
    URLSearchParams.prototype.append = function (k, v) { this._list.push([String(k), String(v)]); this._changed(); };
    URLSearchParams.prototype.set = function (k, v) {
        k = String(k);
        var found = false;
        this._list = this._list.filter(function (p) {
            if (p[0] !== k) return true;
            if (found) return false;
            found = true;
            p[1] = String(v);
            return true;
        });
        if (!found) this._list.push([k, String(v)]);
        this._changed();
    };
    URLSearchParams.prototype.get = function (k) {
        for (var i = 0; i < this._list.length; ++i) if (this._list[i][0] === String(k)) return this._list[i][1];
        return null;
    };
    URLSearchParams.prototype.getAll = function (k) {
        return this._list.filter(function (p) { return p[0] === String(k); }).map(function (p) { return p[1]; });
    };
    URLSearchParams.prototype.has = function (k) { return this.get(k) !== null; };
    URLSearchParams.prototype["delete"] = function (k) {
        this._list = this._list.filter(function (p) { return p[0] !== String(k); });
        this._changed();
    };
    URLSearchParams.prototype.forEach = function (fn, self) {
        for (var i = 0; i < this._list.length; ++i) fn.call(self, this._list[i][1], this._list[i][0], this);
    };
    URLSearchParams.prototype.keys = function () { return this._list.map(function (p) { return p[0]; })[Symbol.iterator](); };
    URLSearchParams.prototype.values = function () { return this._list.map(function (p) { return p[1]; })[Symbol.iterator](); };
    URLSearchParams.prototype.entries = function () { return this._list.map(function (p) { return [p[0], p[1]]; })[Symbol.iterator](); };
    URLSearchParams.prototype[Symbol.iterator] = URLSearchParams.prototype.entries;
    URLSearchParams.prototype.toString = function () {
        return this._list.map(function (p) { return encodePart(p[0]) + "=" + encodePart(p[1]); }).join("&");
    };
    URLSearchParams.prototype._changed = function () { if (this._url) this._url._search = this.toString(); };
    global.URLSearchParams = URLSearchParams;

    // ------------------------------------------------------------ URL
    // Parsed by Qt (QUrl) in C++: {protocol, username, password, hostname,
    // port, pathname, search, hash} or null.
    function URL(input, base) {
        var parts = __native.parseUrl(String(input), base === undefined ? "" : String(base));
        if (!parts) throw new TypeError("Invalid URL: " + input);
        this.protocol = parts.protocol;
        this.username = parts.username;
        this.password = parts.password;
        this.hostname = parts.hostname;
        this.port = parts.port;
        this.pathname = parts.pathname;
        this._search = parts.search;
        this.hash = parts.hash;
        this._params = null;
    }
    Object.defineProperty(URL.prototype, "host", {
        get: function () { return this.hostname + (this.port ? ":" + this.port : ""); }
    });
    Object.defineProperty(URL.prototype, "origin", {
        get: function () { return this.protocol + "//" + this.host; }
    });
    Object.defineProperty(URL.prototype, "search", {
        get: function () { return this._search ? "?" + this._search : ""; },
        set: function (v) { this._search = String(v).replace(/^\?/, ""); this._params = null; }
    });
    Object.defineProperty(URL.prototype, "searchParams", {
        get: function () {
            if (!this._params) { this._params = new URLSearchParams(this._search); this._params._url = this; }
            return this._params;
        }
    });
    Object.defineProperty(URL.prototype, "href", {
        get: function () {
            var auth = this.username ? this.username + (this.password ? ":" + this.password : "") + "@" : "";
            return this.protocol + "//" + auth + this.host + this.pathname + this.search + this.hash;
        }
    });
    URL.prototype.toString = function () { return this.href; };
    URL.prototype.toJSON = function () { return this.href; };
    global.URL = URL;

    // ------------------------------------------------------------ abort, forms, headers
    function AbortSignal() { this.aborted = false; this.reason = undefined; this._listeners = []; this.onabort = null; }
    AbortSignal.prototype.addEventListener = function (type, fn) { if (type === "abort") this._listeners.push(fn); };
    AbortSignal.prototype.removeEventListener = function (type, fn) {
        this._listeners = this._listeners.filter(function (f) { return f !== fn; });
    };
    AbortSignal.prototype.throwIfAborted = function () { if (this.aborted) throw this.reason; };
    AbortSignal.timeout = function (ms) {
        var c = new AbortController();
        global.setTimeout(function () { c.abort(new Error("The operation timed out.")); }, ms);
        return c.signal;
    };
    function AbortController() { this.signal = new AbortSignal(); }
    AbortController.prototype.abort = function (reason) {
        var s = this.signal;
        if (s.aborted) return;
        s.aborted = true;
        s.reason = reason === undefined ? new Error("The operation was aborted.") : reason;
        if (s.onabort) s.onabort();
        s._listeners.forEach(function (fn) { fn(); });
    };
    global.AbortController = AbortController;
    global.AbortSignal = AbortSignal;

    function FormData() { this._list = []; }
    FormData.prototype.append = function (k, v) { this._list.push([String(k), String(v)]); };
    FormData.prototype.set = function (k, v) {
        this["delete"](k);
        this.append(k, v);
    };
    FormData.prototype.get = function (k) {
        for (var i = 0; i < this._list.length; ++i) if (this._list[i][0] === k) return this._list[i][1];
        return null;
    };
    FormData.prototype.has = function (k) { return this.get(k) !== null; };
    FormData.prototype["delete"] = function (k) { this._list = this._list.filter(function (p) { return p[0] !== k; }); };
    FormData.prototype.entries = function () { return this._list.slice()[Symbol.iterator](); };
    global.FormData = FormData;

    function Headers(init) {
        this._map = {};
        if (!init) return;
        if (init instanceof Headers) init = init._map;
        if (Array.isArray(init)) for (var i = 0; i < init.length; ++i) this.set(init[i][0], init[i][1]);
        else for (var k in init) if (Object.prototype.hasOwnProperty.call(init, k)) this.set(k, init[k]);
    }
    Headers.prototype.set = function (k, v) { this._map[String(k).toLowerCase()] = String(v); };
    Headers.prototype.append = function (k, v) {
        var key = String(k).toLowerCase();
        this._map[key] = this._map[key] ? this._map[key] + ", " + v : String(v);
    };
    Headers.prototype.get = function (k) {
        var v = this._map[String(k).toLowerCase()];
        return v === undefined ? null : v;
    };
    Headers.prototype.has = function (k) { return String(k).toLowerCase() in this._map; };
    Headers.prototype["delete"] = function (k) { delete this._map[String(k).toLowerCase()]; };
    Headers.prototype.forEach = function (fn, self) { for (var k in this._map) fn.call(self, this._map[k], k, this); };
    Headers.prototype.entries = function () {
        var m = this._map;
        return Object.keys(m).map(function (k) { return [k, m[k]]; })[Symbol.iterator]();
    };
    global.Headers = Headers;

    // ------------------------------------------------------------ text
    function TextEncoder() { this.encoding = "utf-8"; }
    TextEncoder.prototype.encode = function (s) { return new Uint8Array(__native.utf8Encode(String(s === undefined ? "" : s))); };
    function TextDecoder(label) { this.encoding = label || "utf-8"; }
    TextDecoder.prototype.decode = function (bytes) {
        if (!bytes) return "";
        var buffer = bytes.buffer ? bytes.buffer.slice(bytes.byteOffset || 0, (bytes.byteOffset || 0) + bytes.byteLength) : bytes;
        return __native.decode(buffer, this.encoding);
    };
    global.TextEncoder = TextEncoder;
    global.TextDecoder = TextDecoder;
    global.atob = function (s) { return __native.atob(String(s)); };
    global.btoa = function (s) { return __native.btoa(String(s)); };

    // A little of Intl, for plugins that format dates or numbers with it.
    if (typeof global.Intl === "undefined") {
        global.Intl = {
            DateTimeFormat: function () { this.format = function (d) { return new Date(d).toLocaleString(); }; },
            NumberFormat: function () { this.format = function (n) { return String(n); }; }
        };
    }

    global.window = global;
    global.self = global;
    global.globalThis = global;

    // ------------------------------------------------------------ fetch
    var requests = {};
    function Response(r) {
        this.status = r.status;
        this.ok = r.status >= 200 && r.status < 300;
        this.statusText = r.statusText || "";
        this.url = r.url;
        this.redirected = !!r.redirected;
        this.headers = new Headers(r.headers);
        this._body = r.body;
        this._bytes = r.bytes;
    }
    Response.prototype.text = function () { return Promise.resolve(this._body); };
    Response.prototype.json = function () {
        var body = this._body;
        return new Promise(function (resolve) { resolve(JSON.parse(body)); });
    };
    Response.prototype.arrayBuffer = function () { return Promise.resolve(this._bytes || __native.utf8Encode(this._body)); };
    Response.prototype.clone = function () { return this; };

    function bodyOf(init) {
        var body = init.body;
        if (body === undefined || body === null) return { kind: "", data: "" };
        if (body instanceof FormData) return { kind: "form", data: body._list };
        if (body instanceof URLSearchParams) return { kind: "urlencoded", data: body.toString() };
        if (typeof body === "string") return { kind: "text", data: body };
        if (body instanceof ArrayBuffer) return { kind: "bytes", data: body };
        if (body.buffer instanceof ArrayBuffer) return { kind: "bytes", data: body.buffer };
        return { kind: "text", data: String(body) };
    }
    // fetch(url, {method, headers, body, signal}) through SCANCORE's network
    // (its cookies, so a browser check passed in the app counts).
    function hostFetch(input, init, encoding) {
        init = init || {};
        var url = typeof input === "string" ? input : String(input.href || input.url || input);
        var headers = new Headers(init.headers)._map;
        var body = bodyOf(init);
        return new Promise(function (resolve, reject) {
            var signal = init.signal;
            if (signal && signal.aborted) { reject(signal.reason); return; }
            var id = __native.fetch(url, String(init.method || "GET").toUpperCase(), headers, body.kind, body.data, encoding || "");
            requests[id] = { resolve: resolve, reject: reject };
            if (signal) signal.addEventListener("abort", function () {
                if (!requests[id]) return;
                delete requests[id];
                __native.abortFetch(id);
                reject(signal.reason);
            });
        });
    }
    global.__fetchDone = function (id, result, error) {
        var r = requests[id];
        delete requests[id];
        if (!r) return;
        if (error) r.reject(new Error(error));
        else r.resolve(new Response(result));
    };
    global.fetch = function (input, init) { return hostFetch(input, init); };

    // ------------------------------------------------------------ storage
    // Kept per plugin in SCANCORE's database; values are JSON, with an
    // optional expiry (ms since 1970).
    function Storage() {}
    Storage.prototype.set = function (key, value, expires) {
        var record = { value: value };
        if (expires) record.expires = expires instanceof Date ? expires.getTime() : Number(expires);
        __native.storageSet(String(key), JSON.stringify(record));
    };
    Storage.prototype.get = function (key, raw) {
        var text = __native.storageGet(String(key));
        if (!text) return undefined;
        var record;
        try { record = JSON.parse(text); } catch (e) { return undefined; }
        if (record.expires && Date.now() > record.expires) { this["delete"](key); return undefined; }
        return raw ? record : record.value;
    };
    Storage.prototype["delete"] = function (key) { __native.storageSet(String(key), ""); };
    Storage.prototype.getAllKeys = function () { return __native.storageKeys(); };
    Storage.prototype.clearAll = function () { __native.storageClear(); };
    // The site's own localStorage and sessionStorage (from a web view);
    // SCANCORE has none to give.
    function WebStorage() {}
    WebStorage.prototype.get = function () { return {}; };

    // ------------------------------------------------------------ modules
    var NovelStatus = {
        Unknown: "Unknown",
        Ongoing: "Ongoing",
        Completed: "Completed",
        Licensed: "Licensed",
        PublishingFinished: "Publishing Finished",
        Cancelled: "Cancelled",
        OnHiatus: "On Hiatus"
    };
    var FilterTypes = {
        TextInput: "Text",
        Picker: "Picker",
        CheckboxGroup: "Checkbox",
        Switch: "Switch",
        ExcludableCheckboxGroup: "XCheckbox"
    };
    var vendor = global.__vendor || {};
    var modules = {
        "@libs/fetch": {
            fetchApi: function (url, init) { return hostFetch(url, init); },
            fetchText: function (url, init, encoding) {
                return hostFetch(url, init, encoding).then(function (r) {
                    if (!r.ok) throw new Error("Could not reach " + url + " (" + r.status + ")");
                    return r.text();
                });
            },
            fetchFile: function (url, init) {
                return hostFetch(url, init).then(function (r) { return __native.btoaBytes(r._bytes || __native.utf8Encode(r._body)); });
            },
            fetchProto: function () { return Promise.reject(new Error("This plugin needs protobuf requests, which SCANCORE does not support yet.")); }
        },
        "@libs/novelStatus": { NovelStatus: NovelStatus },
        "@libs/filterInputs": { FilterTypes: FilterTypes },
        "@libs/defaultCover": { defaultCover: "" },
        "@libs/isAbsoluteUrl": { isUrlAbsolute: function (url) { return /^[a-z][a-z0-9+.-]*:\/\//i.test(String(url)); } },
        "@libs/storage": { storage: new Storage(), localStorage: new WebStorage(), sessionStorage: new WebStorage() },
        "@/types/constants": { defaultCover: "" },
        "urlencode": {
            encode: function (s) { return encodeURIComponent(s); },
            decode: function (s) { return decodePart(s); }
        }
    };
    ["cheerio", "htmlparser2", "dayjs", "@libs/aes", "@libs/utils"].forEach(function (name) {
        if (vendor[name]) modules[name] = vendor[name];
    });
    global.__require = function (name) {
        if (Object.prototype.hasOwnProperty.call(modules, name)) return modules[name];
        throw new Error("This plugin needs “" + name + "”, which SCANCORE does not provide.");
    };
    global.__vendorMissing = ["cheerio", "htmlparser2", "dayjs"].filter(function (n) { return !vendor[n]; });

    // ------------------------------------------------------------ calls
    // Runs a plugin method and reports how its promise settles.
    global.__settle = function (id, value) {
        Promise.resolve(value).then(function (result) {
            __native.settle(id, true, result === undefined ? null : result, "");
        }, function (error) {
            __native.settle(id, false, null, String(error && error.message ? error.message : error));
        });
    };
})(this);
