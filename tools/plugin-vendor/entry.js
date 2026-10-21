// What SCANCORE's plugin runtime (resources/plugins/runtime.js) hands
// LNReader plugins as modules. Built into resources/plugins/vendor.js by
// build.sh; run it again after changing a version in package.json.
import { load } from "cheerio";
import { Parser } from "htmlparser2";
import dayjs from "dayjs";
import { gcm } from "@noble/ciphers/aes.js";
import { utf8ToBytes, bytesToUtf8 } from "@noble/ciphers/utils.js";

globalThis.__vendor = {
    "cheerio": { load },
    "htmlparser2": { Parser },
    "dayjs": dayjs,
    "@libs/aes": { gcm },
    "@libs/utils": { utf8ToBytes, bytesToUtf8 },
};
