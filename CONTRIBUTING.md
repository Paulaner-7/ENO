# Contributing to SCANCORE

Thanks for helping. Bug reports go in [Issues](https://github.com/medium-gh/scancore/issues/new/choose), questions and ideas in [Discussions](https://github.com/medium-gh/scancore/discussions), and quick chat on [Discord](https://scancore.onl/discord). Issues labelled [`good first issue`](https://github.com/medium-gh/scancore/labels/good%20first%20issue) are small and well scoped.

## Prerequisites

- CMake 3.28+, Ninja and a C++20 compiler (GCC 13+, Clang 17+ or MSVC 2022)
- Qt 6.9+ with Qt Declarative, Qt SVG, Qt Image Formats and Qt WebEngine (Qt WebEngine is optional; without it the anime player and browser checks are disabled), plus Qt WebView on Windows and macOS, where the anime player uses the system's web view

The README lists the [package commands per system](README.md#build-from-source). Android additionally needs Qt for Android, the Android SDK, NDK r27c and JDK 17 (see [Android](README.md#android)).

## Build and run

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/scancore
```

`SCANCORE_START` opens the app in a given state, which helps with reproducing bugs and taking screenshots:

```bash
SCANCORE_START='{"section":"anime","screenshot":"/tmp/anime.png","delay":8000}' ./build/debug/bin/scancore
```

## Test

```bash
ctest --preset debug
```

Parser tests are offline (`tests/tst_parsing.cpp`). To check every built-in source against the live sites:

```bash
./build/debug/tests/scancore_probe comics "solo leveling"
./build/debug/tests/scancore_probe novels "lord of the mysteries"
```

## Where changes belong

| Change | Where |
| --- | --- |
| A site moved domain, broke, or should be disabled | [`sources/manifest.json`](sources/manifest.json): the domain, `enabled` or a template option. Bump the manifest `version` so installed apps pick it up without a new release. |
| A new site on a known template (WordPress MangaReader, Madara, Iken, NovelFull family) | A new entry in `sources/manifest.json` with the matching `engine` |
| A new kind of comic or novel site | An engine in `src/sources/bundled/comics` or `src/sources/bundled/novels`, registered in `createSource`, plus an offline parser test with its fixture in `tests/fixtures/`. The bundled sources are one separable pack ([its README](src/sources/bundled/README.md)): `-DSCANCORE_WITH_BUNDLED_SOURCES=OFF` builds without them |
| Extensions (repositories, the Kotlin host) | `src/extensions`, `host/` ([host/NOTICE.md](host/NOTICE.md) for third-party code) |
| Anime metadata, episodes, skip times, player links | `src/app/Anime.cpp` |
| Interface | `src/qml` (pages in `pages/`, shared pieces in `components/`, colours and sizes in `theme/`) |
| Networking, database, image cache | `src/core` |

Optional manifest fields for comic and novel entries:

| Field | Meaning |
| --- | --- |
| `language` | The content's language as a short code (`"es"`, `"pt"`); default `"en"`. Shown in Settings → Sources and on the comic source chips, and used by the "All sources" language setting. |
| `urlTag` | For a source that shares a host with another (MangaDex in English and in Spanish): its links carry `#<urlTag>`, so a link opens on the source that made it. Leave it out otherwise; adding it to an existing source changes its ids. |
| `minVersion` | The oldest app version that may load the entry, for entries that need new code. Older apps that know this field skip the entry; 1.4.0 and older do not know it. |
| `options` | Engine options, such as `"languages": ["es", "es-la"]` for `mangadex` or `"edition": "es"` for `webtoon`. |

`SCANCORE_PROBE_MANIFEST=/path/to/trial.json scancore_probe comics "query" <key>` checks entries before they go into the manifest. Sources are added only for public pages: nothing behind a login, a paywall or image encryption.

## Translations

Every string in the interface is wrapped in `qsTr()` and translated in each `i18n/scancore_*.ts`. After changing strings, run `cmake --build --preset debug --target update_translations` and fill in the new entries in every language; a new string should not ship untranslated. A new licence goes into `resources/licenses/` and `components.json` there.

For a source change, include the URL of a series that shows the problem and what you expected to see.

## Pull requests

- Keep each pull request to one change, and match the style of the surrounding code.
- Run `ctest` (and the live probe for source changes) before opening it.
- For interface changes, attach a screenshot. Check the phone layout too: a window narrower than 720 px uses it.
- By contributing you agree that your work is released under the [MIT licence](LICENSE).
