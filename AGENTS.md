# KAI — Architecture & Agent Guidelines

> **IMPORTANT FOR ALL AGENTS**: This file (`AGENTS.md`) is an Always-On Workspace Rule for the **KAI** project (a customized fork of SCANCORE running on macOS Apple Silicon). Read and strictly follow these architectural invariants before planning or making any code changes.

---

## 1. What KAI Is

**KAI** is a native desktop media application (Comics/Manga/Manhwa, Web Novels, and Anime) written in **C++20** and **Qt 6 (Qt Quick / QML)**, forked from `medium-gh/scancore`.
- **Bundle & Identity**: Built as `KAI.app` (`com.vittorio.kai`), display name `KAI`, organization `KAI`.
- **Local Data Isolation**: All user data, SQLite database, cookies, image cache, and backups live under macOS standard paths for `KAI`:
  - App Data: `~/Library/Application Support/KAI/` (`scancore.db`, `cookies.txt`, `manifest.json`)
  - Cache: `~/Library/Caches/KAI/` (`images/`)
  - It is completely isolated from any standalone installation of the original SCANCORE app.

---

## 2. Build & Verification Commands (macOS)

On this machine, `/opt/anaconda3/bin` appears in `PATH` and contains an older Qt installation that shadows Homebrew's Qt 6. **Always** ensure Homebrew's `/opt/homebrew/bin` is prioritized and `CMAKE_PREFIX_PATH=/opt/homebrew` is used when configuring CMake:

```bash
# Configure (only needed if build/ was deleted or CMake presets changed)
PATH="/opt/homebrew/bin:$PATH" cmake --preset debug -DCMAKE_PREFIX_PATH=/opt/homebrew
PATH="/opt/homebrew/bin:$PATH" cmake --preset release -DCMAKE_PREFIX_PATH=/opt/homebrew

# Build Debug (for development and testing) or Release
PATH="/opt/homebrew/bin:$PATH" cmake --build --preset debug
PATH="/opt/homebrew/bin:$PATH" cmake --build --preset release

# Run offline unit tests (MUST pass after every C++/source modification)
PATH="/opt/homebrew/bin:$PATH" ctest --preset debug

# Launch KAI on macOS
open ./build/debug/bin/KAI.app

# Headless/automated UI smoke test & screenshot verification
SCANCORE_START='{"section":"home","screenshot":"/tmp/kai_check.png","delay":1500}' ./build/debug/bin/KAI.app/Contents/MacOS/KAI
```

---

## 3. Structural Architecture (4 Layers)

The codebase is strictly divided into 4 layers. **Never violate layer boundaries.**

### Layer 1: `src/core/` — Infrastructure & Concurrency (`scancore_core` static lib)
*Must remain completely free of QML/UI dependencies (`src/CMakeLists.txt`) so unit tests (`tests/scancore_tests`) can link `scancore_core` directly.*
- **`Database` (`Database.h/.cpp`)**: Single SQLite database (`scancore.db`) in WAL mode. Each thread automatically gets its own thread-local `QSqlDatabase` connection. Provides:
  - Relational tables (`library`, `progress`, `history`, `downloads`, `prefs`, `cache`, `activity`, etc.)
  - Key-value settings via `Database::pref("setting.<key>", fallback)` and `Database::setPref(...)`
  - Expiring metadata cache via `Database::cacheGet(key)` / `Database::cachePut(key, bytes, ttlSeconds)`
- **`Http` & `Cookies` (`Http.h/.cpp`, `Cookies.h/.cpp`)**: Synchronous-style HTTP helpers (`Http::get`, `Http::getJson`, `Http::postJson`) designed to be called **only on worker threads**, with per-host rate limiting, automatic `CancelScope` cancellation when the user leaves a page, and shared `CookieJar` (allowing Cloudflare/browser clearance cookies from `WebCheckView` to be reused by C++ scrapers).
- **`Html` (`Html.h/.cpp`)**: Fast C++ wrapper around `lexbor` for DOM parsing and CSS selector queries.
- **`Task` (`Task.h/.cpp`)**: The bridge between background C++ work and QML.
  - Every asynchronous operation exposed to QML returns `sc::Task *` via `Task::run([=]() -> QVariant { ... })`.
  - QML binds declaratively to `task.loading`, `task.ok`, `task.error`, `task.result`, and `task.verifyUrl`.
  - Dropping or replacing a `Task` in QML (or calling `task.cancel()`) triggers its `CancelFlag`, aborting in-flight HTTP requests immediately.
- **`ImageCache` & `Parallel`**: Disk/memory caching for covers and chapter strips, and bounded parallel execution (`parallelMap`) on worker threads.

### Layer 2: `src/sources/` — Content Engines & Manifest (`scancore_core` static lib)
- **`sources/manifest.json` & `Manifest` (`Manifest.h/.cpp`)**: Declarative catalog of comic and novel sites (IDs, names, domains, languages, engine templates, options). Compiled into Qt resources (`:/sources/manifest.json`).
- **`src/sources/comics/`**:
  - `ComicSource`: Base interface (`search`, `popular`, `latest`, `fetchDetails`, `fetchPages`).
  - `PageEngines.cpp`: HTML-scraping templates (`wp_mangareader`, `madara`, `iken`, etc.).
  - `ApiEngines.cpp`: JSON/REST API adapters (`mangadex`, `webtoon`, `comick`, etc.).
  - `Registry.cpp`: Instantiates sources from `Manifest::current()` and routes URLs via `sourceForUrl`.
- **`src/sources/novels/`**:
  - `NovelSource`: Base interface (`search`, `discover`, `fetchDetails`, `fetchChapter`).
  - `Engines.cpp`: Built-in C++ novel scrapers (`royalroad`, `novelfull`, `readnovelfull`, `freewebnovel`, `libread`).
  - `LnCrawl.cpp`: Optional Python bridge (`resources/lncrawl/bridge.py`) to `lightnovel-crawler` for 130+ extended novel sources.
- **`src/sources/local/`**: Local filesystem readers for CBZ, EPUB (`QZipReader`), folders of images, and PDF (`Qt6::Pdf`).

### Layer 3: `src/app/` — QML Singletons & Application Services (`scancore` executable)
All classes in `src/app/` are registered as `QML_ELEMENT` + `QML_SINGLETON` under the `ScanCore` QML module URI.
- **Crucial Singleton Rule**: Every singleton in `src/app/` has a **private constructor**, a static `ClassName::instance()`, and `static ClassName *create(QQmlEngine *, QJSEngine *) { return instance(); }`. Never make singleton constructors public, otherwise Qt Quick and C++ will instantiate two separate objects!
- Key singletons:
  - `App`: Global settings (`App.settings.<key>`, `App.setSetting(key, val)`), notifications, backups, image/icon URL builders (`image://remote/...`, `image://icon/...`).
  - `Comics`, `Novels`, `Anime`: Media-specific services returning `sc::Task *` to QML pages.
  - `Library`: Saved series, status filters, sorting, background new-chapter polling, and unread counts.
  - `Downloads`: Offline chapter downloader and local storage manager.
  - `Activity`, `Trackers`, `Account`, `Sync`, `Presence`: Reading/watching time statistics, AniList/MAL/Comick OAuth2 PKCE tracking, cloud/WebDAV sync, and Discord Rich Presence.
  - `Customize`: Theme import/export (`.sct`), custom background images/GIFs, translucent window management, and i18n language loading.

### Layer 4: `src/qml/` — Declarative UI (`ScanCore` QML module)
- **QML Singletons** (declared in `src/CMakeLists.txt` with `QT_QML_SINGLETON_TYPE TRUE` and `pragma Singleton` at the top of the file):
  - `Theme.qml`: All color palettes (`light`, `dark`, `black`, `custom`), accents, typography (`fontSans`, `fontDisplay`, `fontSerif`), spacing, and responsive layout breakpoint (`Theme.compact` when window width < 720px).
  - `Motion.qml`: Animation durations and easing curves (automatically respects "Reduce motion").
  - `Router.qml`: Navigation controller (`Router.go(section)`, `Router.push(page, props)`, `Router.back()`, `Router.openSeries(item)`, `Router.openComicReader(...)`, `Router.openNovelReader(...)`, `Router.openWatch(...)`, `Router.toast(msg, kind)`).
  - `Keymap.qml`: Customizable keyboard shortcuts.
- **Shell & Pages**:
  - `Main.qml`: Root `ApplicationWindow`, hosts navigation (`Sidebar`, `Dock`, or compact `TabBar`), per-section `PageStack`s, and the `immersive` stack (used for `ComicReader`, `NovelReader`, `WatchPage`, `SiteCheck`).
  - `src/qml/pages/`: Full-screen section views and detail/reader pages.
  - `src/qml/components/`: Reusable UI primitives (`ScButton`, `CoverCard`, `CoverGrid`, `Shelf`, `SeriesPage`, `SmoothFlickable`, `SettingsPane`, `SettingRow`, etc.).
  - `src/qml/settings/`: Modular category panes loaded inside `SettingsPage.qml`.

---

## 4. Golden Rules for Modifying KAI Safely

When implementing any user request, **never break the existing architecture**. Follow these rules:

1. **Never Block the UI Thread**:
   - All network requests (`Http::*`), HTML parsing (`Html::*`), heavy file I/O, or multi-step database migrations **must** run inside `Task::run([=]() -> QVariant { ... })` or `runInBackground(...)`.
   - QML must interact with asynchronous C++ operations exclusively via `sc::Task *` properties/signals.
2. **Use Existing UI Tokens & Components**:
   - Never hardcode colors, fonts, or animation durations in QML. Always bind to `Theme.*` (`Theme.surface`, `Theme.text`, `Theme.accent`, `Theme.radiusMd`, etc.) and `Motion.*`.
   - Reuse existing components in `src/qml/components/` (`ScButton`, `Chip`, `Select`, `Toggle`, `SettingRow`, `SettingsGroup`, `StateView`, `ConfirmDialog`) so new features look and feel 100% native to KAI.
   - Keep responsive layout in mind: check if the page needs to adapt when `Theme.compact` is true.
3. **Settings Persistence**:
   - To add a new user preference, read it in QML via `App.settings.<myKey>` and write it via `App.setSetting("<myKey>", value)`.
   - `main.cpp` automatically restores all `setting.*` keys from SQLite `prefs` into `App.settings` at startup. Set default values in `App::App()` (`src/app/App.cpp`).
4. **Adding New QML or C++ Files**:
   - QML files under `src/qml/**/*.qml` are picked up via `file(GLOB_RECURSE ... CONFIGURE_DEPENDS qml/*.qml)` in `src/CMakeLists.txt`. If you create a new `.qml` file, re-run CMake or build via `cmake --build --preset debug` (Ninja will re-glob automatically).
   - If you add a new `.qml` singleton, you **must** add `pragma Singleton` in the `.qml` file AND add it to `set_source_files_properties(... PROPERTIES QT_QML_SINGLETON_TYPE TRUE)` in `src/CMakeLists.txt`.
   - New C++ classes in `src/core/` or `src/sources/` go into `scancore_core` in `src/CMakeLists.txt`. New QML-exposed services in `src/app/` go into `qt_add_qml_module(scancore ... SOURCES ...)` in `src/CMakeLists.txt`.
5. **Database Schema Changes**:
   - All SQLite schema definitions live in `Database::open` (`src/core/Database.cpp`) using `CREATE TABLE IF NOT EXISTS` and additive migrations (`ALTER TABLE ...` or new tables). Never drop or destructively alter existing tables (`library`, `progress`, `history`, `downloads`, `prefs`).
6. **Verify Before Finishing**:
   - Always compile (`PATH="/opt/homebrew/bin:$PATH" cmake --build --preset debug`), run unit tests (`PATH="/opt/homebrew/bin:$PATH" ctest --preset debug`), and verify that `KAI.app` launches cleanly without QML errors.

---

## 5. Upstream SCANCORE Updates & Manifest Smart Merge

KAI is designed to receive official upstream improvements from `medium-gh/scancore` **without ever losing or overwriting personal KAI modifications**:

1. **In-App Update Detection (Never Binary Self-Overwrite)**:
   - `Updater::canInstall()` (`src/app/Updater.cpp`) intentionally returns `false`.
   - KAI still checks `api.github.com/repos/medium-gh/scancore/releases/latest` and notifies the user via `UpdateBanner.qml` and `AboutSettings.qml` when a new upstream SCANCORE version is released, but **never** downloads or installs the pre-compiled vanilla `.dmg` (which would erase KAI's custom code).
   - Instead, when the user asks Antigravity to apply an official update, follow the **`sync-upstream` skill** (`.agents/skills/sync-upstream/SKILL.md`) to merge upstream commits at the Git source level while preserving all KAI changes.

2. **Manifest Smart Merge (`src/sources/Manifest.cpp`)**:
   - At runtime, KAI loads `:/sources/manifest.json` (`g_builtIn`) and merges any newer cached or remote upstream `manifest.json` via `mergeWithBuiltIn(incoming, g_builtIn)`.
   - Standard upstream sources automatically receive live domain updates and fixes from `medium-gh/scancore`.
   - Any custom source added to KAI's `sources/manifest.json` with a new `"id"`, or any existing source customized with `"kaiOverride": true` (or `"custom": true`), is **automatically preserved** and never overwritten by upstream manifest refreshes.
