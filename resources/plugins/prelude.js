// Evaluated first in every plugin engine (src/plugins/Runtime.cpp), before
// the bundled libraries and runtime.js: names those look for, and the
// newer built-ins QJSEngine lacks.
var globalThis = this;
var self = this;
var window = this;

(function () {
    "use strict";
    function define(target, name, value) {
        if (!(name in target)) Object.defineProperty(target, name, { value: value, writable: true, configurable: true });
    }
    define(Object, "hasOwn", function (o, k) { return Object.prototype.hasOwnProperty.call(o, k); });
    define(Object, "fromEntries", function (entries) {
        var out = {};
        for (var e of entries) out[e[0]] = e[1];
        return out;
    });
    define(Array.prototype, "at", function (i) {
        i = Math.trunc(i) || 0;
        if (i < 0) i += this.length;
        return i < 0 || i >= this.length ? undefined : this[i];
    });
    define(String.prototype, "at", Array.prototype.at);
    define(Array.prototype, "flat", function (depth) {
        depth = depth === undefined ? 1 : Math.trunc(depth);
        var out = [];
        (function walk(list, d) {
            for (var i = 0; i < list.length; ++i) {
                if (Array.isArray(list[i]) && d > 0) walk(list[i], d - 1);
                else if (i in list) out.push(list[i]);
            }
        })(this, depth);
        return out;
    });
    define(Array.prototype, "flatMap", function (fn, self) { return Array.prototype.map.call(this, fn, self).flat(1); });
    define(Array.prototype, "findLast", function (fn, self) {
        for (var i = this.length - 1; i >= 0; --i) if (fn.call(self, this[i], i, this)) return this[i];
        return undefined;
    });
    define(Array.prototype, "findLastIndex", function (fn, self) {
        for (var i = this.length - 1; i >= 0; --i) if (fn.call(self, this[i], i, this)) return i;
        return -1;
    });
    define(String.prototype, "replaceAll", function (search, replacement) {
        if (search instanceof RegExp) {
            if (!search.global) throw new TypeError("replaceAll needs a global regular expression");
            return this.replace(search, replacement);
        }
        return this.split(String(search)).join(typeof replacement === "function" ? replacement(String(search)) : replacement);
    });
    define(String.prototype, "matchAll", function (re) {
        var flags = re.flags.indexOf("g") >= 0 ? re.flags : re.flags + "g";
        var copy = new RegExp(re.source, flags), s = String(this), out = [], m;
        while ((m = copy.exec(s)) !== null) {
            out.push(m);
            if (m[0] === "") copy.lastIndex++;
        }
        return out[Symbol.iterator]();
    });
    define(String.prototype, "trimStart", function () { return this.replace(/^\s+/, ""); });
    define(String.prototype, "trimEnd", function () { return this.replace(/\s+$/, ""); });
    define(Promise, "allSettled", function (list) {
        return Promise.all(Array.from(list, function (p) {
            return Promise.resolve(p).then(function (value) { return { status: "fulfilled", value: value }; },
                                           function (reason) { return { status: "rejected", reason: reason }; });
        }));
    });
    define(Promise, "any", function (list) {
        return new Promise(function (resolve, reject) {
            var items = Array.from(list), left = items.length, errors = [];
            if (!left) reject(new Error("All promises were rejected"));
            items.forEach(function (p, i) {
                Promise.resolve(p).then(resolve, function (e) {
                    errors[i] = e;
                    if (--left === 0) reject(new Error("All promises were rejected"));
                });
            });
        });
    });
    define(globalThis, "structuredClone", function (v) { return v === undefined ? undefined : JSON.parse(JSON.stringify(v)); });
    define(globalThis, "queueMicrotask", function (fn) { Promise.resolve().then(fn); });
})();
