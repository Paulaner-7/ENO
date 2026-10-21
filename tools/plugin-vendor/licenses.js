// Writes resources/plugins/vendor-licenses.txt: the licence of every
// package esbuild put into vendor.js (from its metafile), for SCANCORE's
// licences screen.
const fs = require("fs");
const path = require("path");
const meta = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const names = new Set();
for (const file of Object.keys(meta.inputs)) {
    if (!file.startsWith("node_modules/")) continue;
    const parts = file.split("/");
    names.add(parts[1].startsWith("@") ? parts[1] + "/" + parts[2] : parts[1]);
}
let out = "Libraries bundled into resources/plugins/vendor.js for LNReader plugins\n" +
          "(built by tools/plugin-vendor).\n";
for (const name of [...names].sort()) {
    const dir = path.join("node_modules", name);
    const pkg = JSON.parse(fs.readFileSync(path.join(dir, "package.json"), "utf8"));
    const file = fs.readdirSync(dir).find((f) => /^(licen[cs]e|copying)(\..*)?$/i.test(f));
    out += "\n" + "=".repeat(72) + "\n" + name + " " + pkg.version + " (" + pkg.license + ")\n" + "=".repeat(72) + "\n\n";
    out += file ? fs.readFileSync(path.join(dir, file), "utf8").trim() + "\n" : "Licence: " + pkg.license + "\n";
}
fs.writeFileSync(path.join("..", "..", "resources", "plugins", "vendor-licenses.txt"), out);
console.log("licences of " + names.size + " packages");
