---
name: sync-upstream
description: >-
  Safely fetches and merges official upstream SCANCORE updates (from medium-gh/scancore)
  into the customized ENO codebase without losing or breaking any personal ENO modifications.
  Activate whenever the user asks to check, pull, sync, or apply an official SCANCORE update.
---

# Syncing Upstream SCANCORE Updates into ENO

Use this procedure whenever the user wants to bring new official updates from `https://github.com/medium-gh/scancore` into **ENO** without overwriting or breaking ENO's custom identity, UI, sources, or features.

---

## Step 1: Ensure Clean Working Tree & Commit Any Unsaved ENO Work

Before fetching or merging upstream changes, check `git status`:
```bash
git status
```
If there are uncommitted local modifications made for ENO, stage and commit them on the local branch first (e.g. `git add -A && git commit -m "chore(eno): save local ENO customizations before upstream sync"`) so Git has a clean snapshot of all ENO work and can perform a 3-way merge.

---

## Step 2: Fetch Upstream Changes & Inspect What Changed

Ensure the `upstream` remote (or `origin`) points to `https://github.com/medium-gh/scancore.git`:
```bash
git remote -v
git fetch origin main --tags
```
Check what new commits and releases exist upstream compared to `HEAD`:
```bash
git log --oneline HEAD..origin/main
git diff --stat HEAD..origin/main
```
Summarize the upstream changes for the user (new features, bug fixes, source adapter changes, version bump).

---

## Step 3: Merge Upstream Without Losing ENO Customizations

Run a no-auto-commit merge first so you can inspect and verify every change before finalizing:
```bash
git merge --no-commit --no-ff origin/main
```

### Mandatory ENO Invariants to Preserve During Merge:
Even if upstream touched these files, **always preserve** ENO's customizations:
1. **`CMakeLists.txt` & `src/CMakeLists.txt`**:
   - Keep `project(ENO VERSION <latest_upstream_version> ...)` (adopt upstream's new `VERSION` number so `Updater` knows ENO is now synced to that version, while keeping project name `ENO`).
   - Keep `OUTPUT_NAME ENO`, `MACOSX_BUNDLE_GUI_IDENTIFIER com.vittorio.eno`, and `MACOSX_BUNDLE_BUNDLE_NAME ENO`.
2. **`src/main.cpp`**:
   - Keep `OrganizationName`, `ApplicationName`, `ApplicationDisplayName`, and system tray labels set to `"ENO"` (`"eno.app"`).
3. **`src/app/Updater.cpp`**:
   - Keep `Updater::canInstall()` returning `false` so ENO never replaces `ENO.app` with a pre-compiled vanilla SCANCORE `.dmg`.
4. **`src/sources/Manifest.cpp`**:
   - Keep `mergeWithBuiltIn(object, g_builtIn)` so custom ENO sources and `"enoOverride": true` entries in `sources/manifest.json` are preserved alongside upstream manifest updates.
5. **UI Branding & Custom Features (`src/qml/`)**:
   - Keep `"ENO"` in `Main.qml`, `Sidebar.qml`, `SettingsPage.qml`, `AboutSettings.qml`, `UpdateBanner.qml`, plus any custom QML pages, components, C++ services, or sources the user has added to ENO.

### Excluded Scope: Manga, Novels & Music Are Never Merged

ENO is anime only (`Router.animeOnly`), so upstream work on the Comics/Manga, Novels and Music sections is **dropped, not merged**. Once the merge is staged (and before committing), pin these paths back to ENO's version:

```bash
git checkout HEAD -- src/sources/comics src/sources/bundled/comics \
                     src/sources/novels src/sources/bundled/novels \
                     src/qml/pages/ComicsPage.qml src/qml/pages/ComicDetails.qml \
                     src/qml/pages/ComicReader.qml src/qml/pages/NovelsPage.qml \
                     src/qml/pages/NovelDetails.qml src/qml/pages/NovelReader.qml \
                     src/qml/settings/ReadingSettings.qml
```

- This also drops comic/novel files upstream adds under those folders; check `git status` for comic/novel additions elsewhere and leave them out too.
- **Music**: upstream added `src/app/Music.*`, `src/app/AudioPlayer.*`, `src/sources/local/AudioTags.*`, the Music/NowPlaying pages, `MusicQueue`, `MiniPlayer`, `SongRow`, `LyricsView`, `docs/music-plugins.md` and their fixtures/tests. Delete them (`git rm -f`), remove their `src/CMakeLists.txt` registrations, and strip every `musicSection` / `setMusicShown` / `MusicQueue` / `MusicPage` reference from `Main.qml`, `Router.qml`, `Onboarding.qml`, `HomePage.qml`, `Retro.qml`, `RetroHub.qml`, `AnimeDetails.qml`, `InterfaceSettings.qml`, `TabBar.qml` and `AnimePage.qml`. Never add a Music nav item, hub section, setting toggle, onboarding card or extension filter.
- `sources/manifest.json`: take upstream's `version`/`updated` and its `anime` block, but keep ENO's `comics`/`novels` lists (`enoExclusive` covers comics). Never add an upstream comic/novel source.
- `tests/fixtures/` and `tests/tst_parsing.cpp`: keep the manga/novel fixtures ENO already has; drop upstream manga/novel/music fixtures and their cases (including `tests/qml/tst_inlineCatalogue.qml` and its `tests/qml/Scancore/Comics.qml`/`Novels.qml` mocks). Anime fixtures and cases are merged normally.
- Everything shared is still merged: `src/core/**`, `src/app/**`, `src/sources/anime/**`, `src/sources/catalogue/**`, `src/sources/local/**`, the build system, i18n and docs.
- If pinning a path breaks the build (a shared header changed under it) or a test, apply the smallest compatibility fix that makes Debug/Release build and `ctest` pass — never to bring manga/novel/music behaviour or entry points back.

---

## Step 4: Compile, Run Tests, and Commit

After resolving any conflicts and verifying all ENO invariants:
```bash
PATH="/opt/homebrew/bin:$PATH" cmake --build --preset debug
PATH="/opt/homebrew/bin:$PATH" ctest --preset debug
PATH="/opt/homebrew/bin:$PATH" cmake --build --preset release
```
Once both Debug and Release compile cleanly and `ctest` passes 100%, commit the merge:
```bash
git commit -m "chore(eno): merge upstream SCANCORE updates while preserving ENO customizations"
```
