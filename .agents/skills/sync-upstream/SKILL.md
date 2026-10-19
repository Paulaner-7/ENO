---
name: sync-upstream
description: >-
  Safely fetches and merges official upstream SCANCORE updates (from medium-gh/scancore)
  into the customized KAI codebase without losing or breaking any personal KAI modifications.
  Activate whenever the user asks to check, pull, sync, or apply an official SCANCORE update.
---

# Syncing Upstream SCANCORE Updates into KAI

Use this procedure whenever the user wants to bring new official updates from `https://github.com/medium-gh/scancore` into **KAI** without overwriting or breaking KAI's custom identity, UI, sources, or features.

---

## Step 1: Ensure Clean Working Tree & Commit Any Unsaved KAI Work

Before fetching or merging upstream changes, check `git status`:
```bash
git status
```
If there are uncommitted local modifications made for KAI, stage and commit them on the local branch first (e.g. `git add -A && git commit -m "chore(kai): save local KAI customizations before upstream sync"`) so Git has a clean snapshot of all KAI work and can perform a 3-way merge.

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

## Step 3: Merge Upstream Without Losing KAI Customizations

Run a no-auto-commit merge first so you can inspect and verify every change before finalizing:
```bash
git merge --no-commit --no-ff origin/main
```

### Mandatory KAI Invariants to Preserve During Merge:
Even if upstream touched these files, **always preserve** KAI's customizations:
1. **`CMakeLists.txt` & `src/CMakeLists.txt`**:
   - Keep `project(KAI VERSION <latest_upstream_version> ...)` (adopt upstream's new `VERSION` number so `Updater` knows KAI is now synced to that version, while keeping project name `KAI`).
   - Keep `OUTPUT_NAME KAI`, `MACOSX_BUNDLE_GUI_IDENTIFIER com.vittorio.kai`, and `MACOSX_BUNDLE_BUNDLE_NAME KAI`.
2. **`src/main.cpp`**:
   - Keep `OrganizationName`, `ApplicationName`, `ApplicationDisplayName`, and system tray labels set to `"KAI"` (`"kai.app"`).
3. **`src/app/Updater.cpp`**:
   - Keep `Updater::canInstall()` returning `false` so KAI never replaces `KAI.app` with a pre-compiled vanilla SCANCORE `.dmg`.
4. **`src/sources/Manifest.cpp`**:
   - Keep `mergeWithBuiltIn(object, g_builtIn)` so custom KAI sources and `"kaiOverride": true` entries in `sources/manifest.json` are preserved alongside upstream manifest updates.
5. **UI Branding & Custom Features (`src/qml/`)**:
   - Keep `"KAI"` in `Main.qml`, `Sidebar.qml`, `SettingsPage.qml`, `AboutSettings.qml`, `UpdateBanner.qml`, plus any custom QML pages, components, C++ services, or sources the user has added to KAI.

---

## Step 4: Compile, Run Tests, and Commit

After resolving any conflicts and verifying all KAI invariants:
```bash
PATH="/opt/homebrew/bin:$PATH" cmake --build --preset debug
PATH="/opt/homebrew/bin:$PATH" ctest --preset debug
PATH="/opt/homebrew/bin:$PATH" cmake --build --preset release
```
Once both Debug and Release compile cleanly and `ctest` passes 100%, commit the merge:
```bash
git commit -m "chore(kai): merge upstream SCANCORE updates while preserving KAI customizations"
```
