<p align="center">
  <img src="resources/branding/scancore.svg" width="128" alt="SCANCORE">
</p>

<h1 align="center">SCANCORE</h1>

<p align="center">
  <b>Comics, novels and anime in one fast, native app.</b><br>
  A reader and player with built-in trackers, for the sources you choose,<br>
  with your library, history and downloads kept on your own device.
</p>

<p align="center">
  <a href="https://github.com/medium-gh/scancore/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/medium-gh/scancore?label=release&labelColor=111113&color=f2f2f3&style=flat"></a>
  <a href="https://github.com/medium-gh/scancore/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/medium-gh/scancore/total?label=downloads&labelColor=111113&color=f2f2f3&style=flat"></a>
  <a href="https://github.com/medium-gh/scancore/actions/workflows/ci.yml"><img alt="CI" src="https://img.shields.io/github/actions/workflow/status/medium-gh/scancore/ci.yml?branch=main&label=build&labelColor=111113&style=flat"></a>
  <a href="LICENSE"><img alt="MIT licence" src="https://img.shields.io/badge/licence-MIT-f2f2f3?labelColor=111113&style=flat"></a>
  <img alt="Platforms" src="https://img.shields.io/badge/Linux%20·%20Windows%20·%20macOS%20·%20Android-111113?style=flat">
</p>

<p align="center">
  <a href="#download">Download</a> ·
  <a href="#system-requirements">Requirements</a> ·
  <a href="#features">Features</a> ·
  <a href="#screenshots">Screenshots</a> ·
  <a href="#community">Community</a> ·
  <a href="#build-from-source">Build from source</a> ·
  <a href="#sources">Sources</a> ·
  <a href="#faq">FAQ</a>
</p>

![SCANCORE home](docs/screenshots/home.webp)

> [!NOTE]
> SCANCORE is free and open source, built by [Medium Dev](https://mediumdev.com). The official downloads are the ones on this repository's [releases page](https://github.com/medium-gh/scancore/releases). The companion website is [scancore.onl](https://scancore.onl), and the community hangs out on [Discord](https://scancore.onl/discord).

## Why SCANCORE

- **Native and fast.** Written in C++20 with Qt Quick, so it renders on the GPU and starts quickly. There is no Electron and no embedded web UI.
- **Your sources, your choice.** SCANCORE runs no extension repository. It reads your files, a few official and public catalogues, a bundled pack of built-in sources (which can be switched off) and the sources you add yourself.
- **Trackers built in.** AniList, MyAnimeList and Comick keep your lists in step as you read and watch, and their catalogues are there to browse.
- **Three kinds of media, one library.** Comics, novels and anime share the same library, history, downloads and search.
- **Local first.** No account and no server needed. Everything lives in one SQLite file you can back up.
- **Smooth.** Covers fly into their details page, sections and pages glide, readers scroll with easing. Animation speed is adjustable, and a "Reduce motion" switch turns all of it off.
- **Updates itself.** New versions download in the background and install when you restart, on every platform.

## Features

### Comics
- Search **every source you use at once**, ranked by how well each title matches, or browse **Comick's catalogue** and open a title from whichever source has it.
- **Vertical strip** for webtoons and manhwa, or **page by page** for manga, left-to-right or right-to-left.
- Adjustable strip width (Ctrl + scroll), panel gaps, a progress rail, and very tall strips that stay sharp. In page mode, swipe or drag to turn pages.
- The next chapter is fetched ahead of time, and your exact place is remembered in every chapter.
- Latest-updates feed across sources, and your recent searches one click away. Paste a series link to open it directly.

### Novels
- **Royal Road** discovery shelves (top rated, rising stars, trending, new and completed), EPUB books on your device, and search across every novel source you use.
- A reader you can tune: **serif, sans or grotesk** type, size, line spacing, column width, justification, and **app, paper, sepia, dark and black** themes.

### Anime
- **AniList**-powered trending, seasonal, popular and top lists, a rotating spotlight, search with genre, year, format and status filters, and a 7-day **airing schedule**.
- Details with episode thumbnails and titles, related entries and recommendations.
- Episodes play inside the app from the sources you add, with **resume**, **auto-next** (with a countdown you can set) and **AniSkip** intro, ending and recap skipping, automatic or on a button.

### Library and everything else
- **Library** with new-chapter checks in the background, desktop notifications and four ways to sort.
- **Home** picks up where you left off: the last thing you read or watched, one click from carrying on.
- **History** grouped by day and searchable, **offline downloads** for comic and novel chapters, a **Ctrl+K command palette** with quick commands, and every keyboard shortcut on one sheet (press **?**), each one changeable in Settings → Keyboard.
- **Optional scancore.onl account**: sign in or sign up from the welcome screen or Settings to bring your website library and comic reading history into the app and send saves back.
- **Browser checks**: if a site asks for one, open it inside SCANCORE once and the app reuses the clearance (desktop).
- **Make it yours**: light, dark and black themes, or your own: a window colour from a colour wheel, any accent, a picture or animated GIF behind everything, see-through panels and window. Themes share as single `.sct` files that work on every system.
- Navigation as a sidebar, floating sidebar, icon rail or dock, on either side; adjustable text and cover sizes and animation speed, a choice of start page, backup and restore, tray icon and keyboard control throughout.
- **Phone layout** on Android: bottom tabs, stacked details pages and touch controls.

## Screenshots

| Welcome | Comic details |
| --- | --- |
| ![Welcome](docs/screenshots/welcome.webp) | ![Comic details](docs/screenshots/comic-details.webp) |
| **Comic reader** | **Novel reader** |
| ![Comic reader](docs/screenshots/comic-reader.webp) | ![Novel reader](docs/screenshots/novel-reader.webp) |
| **Anime** | **Anime details** |
| ![Anime](docs/screenshots/anime.webp) | ![Anime details](docs/screenshots/anime-details.webp) |
| **Novels** | **Account** |
| ![Novels](docs/screenshots/novels.webp) | ![Account](docs/screenshots/welcome-account.webp) |

<p align="center">
  <img src="docs/screenshots/phone-home.webp" width="240" alt="Phone home">
  <img src="docs/screenshots/phone-details.webp" width="240" alt="Phone details">
  <img src="docs/screenshots/phone-reader.webp" width="240" alt="Phone reader">
</p>

## Download

Get the latest version from the **[releases page](https://github.com/medium-gh/scancore/releases/latest)**.

| Platform | File | Notes |
| --- | --- | --- |
| **Linux** (x86_64) | `SCANCORE-<version>-linux-x86_64.AppImage` | Runs on any recent distribution. Updates itself in place. |
| **Linux** (Flatpak) | `SCANCORE-<version>-linux-x86_64.flatpak` | Install with Flatpak; updates are handled by Flatpak. Requires the Flathub runtime. |
| **Windows** 10/11 (x64) | `SCANCORE-<version>-windows-x64-setup.exe` | Installer with Start menu entry. Updates itself. |
| | `SCANCORE-<version>-windows-x64-portable.zip` | No install; unzip and run `scancore.exe`. |
| **macOS** 13+ (Apple Silicon and Intel) | `SCANCORE-<version>-macos-universal.dmg` | Drag to Applications. Updates itself. |
| **Android** 9+ (arm64) | `SCANCORE-<version>-android-arm64.apk` | Sideload; updates open the system installer. |

Community package definitions for [AUR, WinGet, Scoop and Homebrew](docs/package-managers.md) are maintained separately from GitHub release uploads.

### System requirements

| Platform | Needs |
| --- | --- |
| **Linux** | x86_64 distribution from 2022 or later (glibc 2.35+, since the AppImage is built on Ubuntu 22.04) and FUSE 2 for the AppImage; or any distribution with Flatpak |
| **Windows** | Windows 10 or 11, 64-bit. Anime plays in Microsoft Edge WebView2, which Windows 11 includes and Windows 10 gets with Edge |
| **macOS** | macOS 13 or later, Apple Silicon or Intel |
| **Android** | Android 9 or later, arm64 |

An internet connection is needed to browse sources; downloaded chapters read offline.

<details>
<summary><b>Install notes for each platform</b></summary>

**Linux.** Make the AppImage executable and run it:

```bash
chmod +x SCANCORE-*-linux-x86_64.AppImage && ./SCANCORE-*-linux-x86_64.AppImage
```

To show it in your app launcher (GNOME, KDE, rofi, wofi and others), keep it somewhere permanent and add a desktop entry:

```bash
mkdir -p ~/Applications ~/.local/share/applications ~/.local/share/icons/hicolor/scalable/apps
mv SCANCORE-*-linux-x86_64.AppImage ~/Applications/SCANCORE.AppImage
curl -fsSL https://raw.githubusercontent.com/medium-gh/scancore/main/resources/branding/scancore.svg \
  -o ~/.local/share/icons/hicolor/scalable/apps/com.mediumdev.scancore.svg
printf '[Desktop Entry]\nType=Application\nName=SCANCORE\nExec=%s %%U\nIcon=com.mediumdev.scancore\nCategories=Graphics;Viewer;\n' \
  "$HOME/Applications/SCANCORE.AppImage" > ~/.local/share/applications/com.mediumdev.scancore.desktop
```

AppImages need FUSE 2 (`libfuse2` on Debian and Ubuntu, `fuse2` on Arch).

**Flatpak.** Install Flatpak and configure the Flathub remote for your distribution, then install the downloaded bundle:

```bash
flatpak install --user ./SCANCORE-*-linux-x86_64.flatpak
```

The bundle contains SCANCORE; Flatpak downloads the KDE runtime and Qt WebEngine (the anime player) from Flathub. Future app updates are provided by Flatpak, not the in-app updater.

**Windows.** Run the installer. SmartScreen may warn about an unrecognised app the first time: choose *More info*, then *Run anyway*.

**macOS.** Open the disk image and drag SCANCORE to Applications. The app is not notarised yet, so the first time, right-click it and choose *Open*.

**Android.** Download the APK on your phone and open it. Allow installs from your browser or file manager when asked. Updates are offered inside the app.

</details>

## Sources

SCANCORE is a reader. It hosts no content and runs no extension repository. It reads only what sites show publicly and does not bypass paywalls, logins or locked chapters.

| Kind | Built in |
| --- | --- |
| Comics | Files on this device (folders of images, CBZ, CBR, 7z, PDF), Webtoon, MangaDex; Comick's catalogue for browsing |
| Novels | EPUB books on this device, Royal Road |
| Anime | AniList metadata, ani.zip episode details, AniSkip |

Besides these, SCANCORE includes a **bundled pack** of built-in comic and novel sources. It is kept apart from the rest of the app on purpose (`src/sources/bundled`, `sources/bundled.json`): it can be switched off remotely, left out with the `SCANCORE_WITH_BUNDLED_SOURCES` build option, or removed in a single commit, because the app mainly depends on its built-in trackers and on the extensions you add, not on the pack. Everything else comes from sources you add yourself. SCANCORE 2.0 runs the extensions people already use: Mihon and Tachiyomi extensions for comics, LNReader plugins for novels and Aniyomi extensions for anime, from repositories you choose. SCANCORE asks you to trust each extension's signing key before it runs.

Rights holders: see [TAKEDOWN.md](TAKEDOWN.md).

## Build from source

### Desktop (Linux, Windows, macOS)

Requirements: CMake 3.28+, Ninja, a C++20 compiler (GCC 13+, Clang 17+ or MSVC 2022), and **Qt 6.9 or newer** with Qt Quick, Qt Quick Controls, Qt SVG, Qt Image Formats and **Qt WebEngine** (for the anime player; optional). [lexbor](https://github.com/lexbor/lexbor), the HTML parser, is downloaded automatically. A JDK 17 or newer, if installed, also builds the extension host (`host/`, with its Gradle wrapper) for development and its tests; without one the app builds without it.

```bash
git clone https://github.com/medium-gh/scancore.git && cd scancore
cmake --preset debug          # or: cmake --preset release
cmake --build --preset debug
ctest --preset debug
./build/debug/bin/scancore
```

Installing the dependencies:

| System | Command |
| --- | --- |
| Arch Linux | `sudo pacman -S cmake ninja qt6-base qt6-declarative qt6-svg qt6-imageformats qt6-webengine` |
| Fedora | `sudo dnf install cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qtimageformats qt6-qtwebengine-devel` |
| Ubuntu, Debian | Needs an archive with Qt 6.9 or newer (otherwise use the [Qt Online Installer](https://www.qt.io/download-qt-installer)): `sudo apt install cmake ninja-build qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-image-formats-plugins qt6-webengine-dev` |
| macOS | `brew install cmake ninja qt` |
| Windows | Install Qt 6.9+ for MSVC 2022 with the [Qt Online Installer](https://www.qt.io/download-qt-installer) (tick Qt WebEngine, Qt WebView and Qt Image Formats), then build from a *Developer PowerShell for VS 2022* with `-DCMAKE_PREFIX_PATH=C:\Qt\6.x.y\msvc2022_64`. |

Build options: `-DSCANCORE_WITH_WEBENGINE=OFF` builds without the embedded browser. The anime player and browser checks are then unavailable, and everything else works.

Anime episodes are H.264 video. Distribution and Flathub builds of Qt WebEngine can play it, but the Qt WebEngine in the Qt Online Installer cannot. On Windows and macOS the player therefore uses the system's web view through Qt WebView (WebView2 or WKWebView); `-DSCANCORE_WITH_SYSTEM_WEBVIEW=OFF` turns that off. On Linux, build against your distribution's Qt WebEngine, or one configured with `-DFEATURE_webengine_proprietary_codecs=ON` (see `.github/workflows/qtwebengine.yml`).

### Android

Requirements: the desktop Qt above (as the host), **Qt for Android** (same version, `android_arm64_v8a`, with Qt WebView and Qt Image Formats), the Android SDK, **NDK r27c** (`27.2.12479018`) and JDK 17.

```bash
export ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/27.2.12479018
~/Qt/6.11.2/android_arm64_v8a/bin/qt-cmake -S . -B build/android -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH=/usr -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_NDK_ROOT=$ANDROID_NDK_ROOT
cmake --build build/android --target apk
adb install build/android/src/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

`QT_HOST_PATH` is your desktop Qt prefix: `/usr` on Arch, or something like `~/Qt/6.11.2/gcc_64` from the Qt installer. Use `-DCMAKE_BUILD_TYPE=Debug` for an APK that installs without signing. OpenSSL for Android is fetched automatically.

### Releases

Pushing a tag such as `v1.2.0` runs [the release workflow](.github/workflows/release.yml). It builds the Linux AppImage, the Windows installer and portable zip, the macOS universal disk image and a **signed** Android APK, then publishes them as a GitHub release. Installed copies pick the release up through their updater.

## How it works

```
src/core           HTTP (per-site limits, cancellation), HTML parsing, SQLite, image cache, background tasks
src/sources        built-in comic and novel sources, files on this device, manifest loading
src/app            services used by the interface: Comics, Novels, Anime, Library, Downloads,
                   Account, Updater, Extensions and App
src/qml            the interface: theme and motion tokens, components, pages, phone layout
sources/           the source manifest
tests/             offline parser tests and the live source probe
packaging/         Linux desktop entry, Windows installer, macOS and Android icons and manifest
```

- Every network request runs on a worker pool with per-site and overall limits. Leaving a page cancels its requests.
- Chapter pages are decoded off the interface thread and drawn as GPU tiles, so long webtoon strips stay sharp.
- The library, progress, history, downloads, preferences and a metadata cache share one SQLite database (`~/.local/share/Medium Dev/ScanCore` on Linux).

## Community

Join the [SCANCORE Discord](https://scancore.onl/discord) for questions, feedback and community updates. Report bugs and request features in [GitHub Issues](https://github.com/medium-gh/scancore/issues); code contributions are welcome through pull requests.

## Contributing

Issues and pull requests are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) has the build, test and source-adapter details. Questions and ideas go in [Discussions](https://github.com/medium-gh/scancore/discussions), bug reports in [Issues](https://github.com/medium-gh/scancore/issues), and chat on [Discord](https://scancore.onl/discord). Release notes are in [CHANGELOG.md](CHANGELOG.md).

- **A bug or a feature?** Open an issue first for anything larger than a small fix.
- **Sources** for scan sites are not accepted as pull requests (see [Sources](#sources)).
- Run `ctest` and the live probe before opening a pull request.

## FAQ

<details>
<summary><b>Do I need an account?</b></summary>

No. Everything works without one. A [scancore.onl](https://scancore.onl) account only adds syncing with the website.
</details>

<details>
<summary><b>Where is my data?</b></summary>

On your device, in the app data folder (`~/.local/share/Medium Dev/ScanCore` on Linux, `%APPDATA%\Medium Dev\ScanCore` on Windows, `~/Library/Application Support/Medium Dev/ScanCore` on macOS). Settings → Backup exports it all to one file.
</details>

<details>
<summary><b>A source shows "asked for a browser check".</b></summary>

On desktop, use the **Open browser check** button (or the globe next to the source in Settings → Sources), complete the site's check and press **Done**. SCANCORE then reuses the clearance.
</details>

<details>
<summary><b>Can I read in Spanish or another language?</b></summary>

Yes. The interface is in eighteen languages besides English (Settings → Interface → Language, or the first screen), and SCANCORE turns on the sources in your language by default; Settings → Sources → Source languages adds others. "Read from" finds a series under its name in your language, and when no source in your language has it, offers to look in another.
</details>

<details>
<summary><b>How do I add extensions?</b></summary>

Settings → Extensions → Add repository, or tap a repository link (scancore://, tachiyomi://, mihon://, aniyomi:// and lnreader:// links open in SCANCORE). Comic, anime and novel extensions work on Android and desktop; on desktop SCANCORE downloads a small Java runtime for comic and anime extensions the first time.
</details>

<details>
<summary><b>Can I bring my Mihon library, and take it back?</b></summary>

Settings → Connections imports a Mihon or Tachiyomi backup (series land on the same extension source when it is installed), a public AniList list, a MyAnimeList export or a Comick library. **Back up for Mihon** saves the comics you read from extensions as a backup Mihon restores.
</details>

<details>
<summary><b>Where do episodes come from?</b></summary>

From the anime sources you add. SCANCORE itself has AniList's catalogue, schedule and details, and plays what your sources provide. The player shows a message rather than a broken video when a source has no episode.
</details>

<details>
<summary><b>How do I turn off automatic updates?</b></summary>

Settings → About → *Install updates automatically*. You can also turn off update checks entirely.
</details>

## Licence

SCANCORE is released under the [MIT licence](LICENSE).

Fonts: DM Sans, Space Grotesk and Literata (SIL Open Font Licence). Icons: [Lucide](https://lucide.dev) (ISC). HTML parsing: [lexbor](https://github.com/lexbor/lexbor) (Apache 2.0). Video player: [mpv](https://mpv.io) and FFmpeg (LGPL). The extension host and the rest are listed, with their licences, in Settings → About → Licences and [host/NOTICE.md](host/NOTICE.md).

## Disclaimer

SCANCORE does not host any content. Its bundled sources are a separable pack. Extensions you add are other people's code, run at your choice. SCANCORE's developers are not affiliated with any site or extension. Support the creators and official releases of the series you enjoy.
