# Changelog

Downloads for the current version are listed on the [releases page](https://github.com/medium-gh/scancore/releases).

## 2.11.0 — 2026-10-24

Switch from Mihon without losing your downloads, a progress bar you can make your own, and web views for extensions on computers.

- **Mihon and Komikku downloads, read where they are**: choose Mihon's storage folder (or its `downloads` folder) in Settings → Connections → Chapters Mihon downloaded, or right in the backup import. Each downloaded series is matched to the same series from your Mihon backup (by source and title, and by Mihon's own chapter fingerprints when a title was renamed), and each chapter to its chapter. They then count as downloads: they open offline, your progress is kept, and nothing is copied, so 50 GB of chapters do not take another 50 GB. Series not in your library show up under On this device. Works with Android's folder picker too (one quick look per folder: 9,000 chapters are linked in about 6 seconds on a slow test phone), and SCANCORE looks again at each start for chapters Mihon added. The Downloads page shows them as one row per series, and removing one there only stops using the files; Mihon's files are never deleted.
- **The built-in anime player no longer turns into an ad site**: on Android, tapping the player could open an advertiser's page inside it instead of the episode. Pop-ups are refused, the player page cannot be sent elsewhere, and taps on web players now go to SCANCORE's controls (double tap the sides to skip 10 seconds, the middle to pause; hold to reach the site's own menus for a few seconds).
- **A progress bar your way**: three looks for the comic reader's progress bar (a piece per page, one line, or a floating bar with the page number), on the left, right or bottom edge, three thicknesses, your own colours for pages read, unread and current (or one of five sets), and how see-through it is. Settings → Reading shows each one as you change it.
- **Welcome screens**: pick your progress bar (or none) by looking at them, then choose which notifications you want and allow them.
- **Extensions that open a web view now work on computers** (Comix and other newer Keiyoushi sources): SCANCORE downloads the browser they need (about 250 MB, once, the first time such a source is used) and runs it out of sight, as Suwayomi does. Comix's search and chapter lists work on Windows, macOS and Linux. Its chapter pages fail everywhere right now (Android too) after a change on the site; that needs a new version of the extension. Computers get the new Extensions runtime 5 by themselves.
- **Extension updates are easy to find**: Extensions → Installed starts with what is ready to update and an Update all button, or says everything is up to date with when it last checked and a Check for updates button. You get a notification when trusted extensions update by themselves.
- **Browse extensions your way**: under Available, filter by comics, novels or anime, by repository and 18+, beside the language. A repository opens its own list.
- **All sources finds titles in your extensions too**: a search through the catalogue (Comick, AniList or MyAnimeList) now also searches your installed extensions and shows what they have below, as it comes in. Searching many sources at once no longer opens browser checks over the app; a source that needs one gets a button instead.
- **Anime from AniList and MyAnimeList play from the built-in servers** by default, without first searching every installed extension. Settings → Anime → Play from switches to your extensions or to asking, and which extensions to look in. Watch from lets you tick the extensions it searches.
- **Anime pages on wide screens** show the details in a column beside the episodes, each scrolling by itself.
- **Extension log** (computers): Settings → Sources → Extension log opens what extensions reported, errors included, for bug reports.
- Fixed: a second grid of covers lower on a page (such as extension results) stayed empty while scrolling.
- Not yet checked on real devices: Mihon downloads in a real Mihon folder on a phone (tested with Mihon's layout on the Android emulator and on Linux), the extension web view on Windows and macOS (tested on Linux), the ad fix on a phone where an ad appears (the emulator showed none), and Watch from's extension choices with real anime extensions.

## 2.10.0 — 2026-10-23

Lists your way, Komikku backups, a player that stays open, and smoother on slow phones.

- **Built-in lists can be renamed, reordered and hidden** ([#9](https://github.com/medium-gh/scancore/issues/9)): Reading, Plan to read, On hold, Completed and Dropped can't be deleted, because trackers, sync and backups rely on them. You can now give them your own name, move them among your own lists and hide them from the library (long-press or right-click a list chip, or Settings → Library → Your lists). A hidden list's series stay on it and under All. The layout syncs between your devices, and Mihon backups keep the names you chose.
- **Komikku and TachiyomiSY backups** ([#9](https://github.com/medium-gh/scancore/issues/9)): merged entries come in as their main source, series written twice in the backup come in once with everything both copies knew, hidden categories become hidden lists, and edited titles still find their series. Saved searches and edited covers and descriptions stay in the other app, and the import summary says how many. The import dialog and file picker name Komikku and TachiyomiSY.
- **Smoother on slow phones** ([#9](https://github.com/medium-gh/scancore/issues/9)): saving your place while reading made every page behind the reader reload its list. Pages now catch up once when you return. Long chapter lists reuse their rows (about 70% less work per scroll step in our measurements), and a crash when scrolling covers fast is fixed.
- **The anime player no longer closes when you press it**: the hidden back button took clicks at the top-left of the video, and the back arrow on the landscape rail sat where double taps seek. Hidden controls now ignore presses, and the first tap only shows them. The side rail is gone: close is at the top-left, and previous and next episode sit with the other controls. Android's Back leaves full screen before it closes the player.
- **One player settings panel**: Quality, Audio, Subtitles, Speed, the server, episodes and the page's actions (download, cast, other app…) are in one dark panel. It opens as a bottom sheet on phones and scrolls instead of running off a short screen.
- **Fit the picture** (asked on Discord): Fit, Crop to fill (no black bars), Stretch or Original size, from the player's settings or Settings → Anime.
- **Player gestures on phones**: swipe up or down on the left for brightness and on the right for volume, swipe sideways to seek (with a time preview), double tap the sides to skip 10 seconds or the middle to pause, and hold for 2× speed. Each one can be turned off in Settings → Anime → Gestures.
- **Series listed once per uploader** (Kagane): search, "Read from" and moving a series now pick the fullest edition, not the first one found, and results show how many chapters each has.
- **Keiyoushi extensions show their chapters**: Comix and other newer Keiyoushi sources failed with "getMangaUpdate must not be called concurrently for same manga". Details and chapters for a series are now one request. Sources that page through a site slowly get five minutes instead of two. Computers get the new Extensions runtime 4 by themselves.
- **Sources that need Android's web view** (such as Comix) now say on computers that they work on Android, instead of failing with a vague error.
- Not yet checked on real devices: the player gestures (tested on the Android emulator), keeping the screen on with Android, Windows and macOS, the 32-bit ARM (armv7) Android package, and the Quality menu with a real extension's stream.

## 2.9.0 — 2026-10-23

Mihon backups that bring back everything, Kagane, and chapters your way.

- **Extensions on Windows, macOS and Linux** ([#20](https://github.com/medium-gh/scancore/issues/20)): "Browse this source" no longer fails with "Cannot read field mQueue because looper is null". Extensions such as Komga, Comix and others that hand work to Android's main thread now run, and Komga's settings open. Computers get the new Extensions runtime 3 by themselves.
- **Multiple-choice extension settings save** ([#21](https://github.com/medium-gh/scancore/issues/21)): ticking a genre under Blocked genres (or any setting with several choices) failed with "JsonNull is not a JsonArray"; it never saved on any platform. It does now.
- **Settings no longer reset on restart**: settings made of lists (custom keyboard shortcuts, custom theme colours, recent searches, a series' own reader settings and others) were kept only until SCANCORE closed. They now stay.
- **Mihon backups bring back everything**: every Mihon category becomes one of your lists in Mihon's order (empty ones too), with each series' date added, history (so Continue lands on the chapter you read last), every chapter you left part-way on its page, bookmarks, the chapter list's sort and filters, the reading mode you chose for it, Kitsu, MangaUpdates, Shikimori and Bangumi links, and the groups you hid (a new **Hide groups** filter on the series page). Before importing, SCANCORE offers to add the backup's extension repositories and install the extensions its series came from, so they come back on their own sources. A series in several Mihon categories goes to the first, and the import says how many.
- **And back to Mihon**: Back up for Mihon now keeps bookmarks, the page you are on, chapter groups and dates, chapter and reader settings, hidden groups, tracker links and your repositories, and Reading is Mihon's Default. Checked with Mihon 0.21: a library sent there and back comes back the same.
- **Kagane** as a built-in source, read with SCANCORE's own engine, in English and 14 more languages (adult titles left out).
- **Chapters as a grid**: short chapter titles (volumes such as 第01卷191p) can show two to a line on phones and more on wide screens, per series from the grid button above the chapters, or for all in Settings → Reading → Chapter layout. Chapter titles now take two lines instead of being cut off.
- **Volumes, Chapters and Extras**: when a series has more than one kind (卷/巻/Vol. volumes, 话/話/Ch. chapters, 番外/特别篇/Extra/Special/Omake extras), tabs above its chapters show one kind at a time.
- **Hold to zoom**: in the comic reader, hold a finger or the mouse on the page to see it twice as big until you let go, under your finger or at the centre (Settings → Reading). Double tap to zoom can be switched off there.

## 2.8.0 — 2026-10-23

Your own library lists, a quality and dub choice for anime, and the screen stays on.

- **Extensions work on Windows, macOS and Linux again** ([#14](https://github.com/medium-gh/scancore/issues/14)): every extension from Keiyoushi's repository said "This extension uses lib {4:1070386381}, which this SCANCORE does not run" (in Source settings and when loading). The fix was in the app since 2.1.1, but the Extensions runtime that computers download separately was older than it, so it never arrived; that older runtime could not run anime extensions either. SCANCORE now asks for a new runtime and replaces an older one by itself in the background, and a release can no longer go out with a runtime older than its extension host.
- **The screen stays on while you watch or read**: phones no longer dim and lock in the middle of an episode, and computers do not start the screensaver while a video plays (Android, Windows, macOS, and Linux desktops that offer the screensaver or portal inhibit). Readers keep the screen on too; Settings → Reading → Keep the screen on while reading turns that off.
- **Choose the quality** (asked on Discord): Settings → Anime → Preferred quality (best, 1080p, 720p, 480p or 360p) picks which of a source's videos plays first and which resolution a stream with several starts on. A new Quality button on SCANCORE's player lists the stream's resolutions and the server's other videos while you watch.
- **Dubs** ([#15](https://github.com/medium-gh/scancore/issues/15)): Settings → Anime → Preferred audio picks a dub (English, Spanish, Portuguese, French, German, Italian or Hindi) or the original Japanese. Videos and servers are chosen by their names (Dub, Sub, English…), and streams with several audio tracks start on that language.
- **Your own library lists** ([#9](https://github.com/medium-gh/scancore/issues/9)): add lists beside Reading, Plan to read, On hold, Completed and Dropped with New list in the library or Settings → Library → Your lists; rename, reorder or remove them by long-pressing or right-clicking their chip. They sync between your devices, new ones are checked for new chapters, and Mihon and LNReader backups bring their own categories along (and SCANCORE's Mihon export takes yours).
- **Compact grid with titles on the covers** ([#9](https://github.com/medium-gh/scancore/issues/9)): the library's compact grid now shows each title over the foot of its cover, more covers to a row.
- **One chapter list** ([#17](https://github.com/medium-gh/scancore/issues/17)): a comic's or novel's chapters are one list that loads as you scroll, as anime episodes are, instead of ten to a page. Settings → Reading → Show chapters brings the pages of ten back.
- **Reversing the chapter order keeps your place** ([#18](https://github.com/medium-gh/scancore/issues/18)): it no longer jumps to the top of the page, and the page stays at its top while its details load.
- **32-bit Android phones** ([#19](https://github.com/medium-gh/scancore/issues/19)): a separate `android-armv7.apk` for older phones that cannot install the arm64 one, which then updates itself like the other.
- **Open a book file from the Novels page** ([#16](https://github.com/medium-gh/scancore/issues/16)): the folder button opens an EPUB in the novel reader (or a PDF as pages), beside the folders in Settings → Sources → On this device.
- The Flatpak is published again (2.7.1's build pointed at a lexbor tag that does not exist).

## 2.7.1 — 2026-10-21

- **Status bar icons match SCANCORE's theme on Android**: with the phone in light mode, the clock, battery and signal icons were dark on SCANCORE's dark screens and hard to see. They now follow the app's theme (light icons on dark, dark icons on light), including the navigation bar, and the readers and player choose by their own background (the novel reader by its reading theme).

## 2.7.0 — 2026-10-21

- **Anime works while AniList is down**: AniList switched off its service on 21 October, leaving the Anime page saying "Anime details are unavailable right now". SCANCORE now fills the page, anime details, search, episode lists and the schedule from Kitsu and ani.zip whenever AniList does not answer, with the same anime, so your library, progress and sync carry on. Recommendations come back when AniList does. When nothing can be reached, the page shows what it loaded last time, or says why.
- **The MyAnimeList catalogue keeps working too**: its lists and its spotlight fall back the same way when MyAnimeList or AniList is down.
- **A full screen button in the player**: SCANCORE's own video player has one at the end of its controls; on phones it turns the video to landscape.
- **The spotlight's shade covers the whole picture while swiping**: on phones, a bright strip of the banner showed at the edge as you swiped.
- **Sharper comic pages**: pages are decoded at exactly the size they appear on screen and drawn pixel for pixel, instead of being shrunk or stretched by the graphics card. Paged reading is noticeably crisper, and pages no longer go slightly soft between screen pixels while a strip scrolls.
- **Crisper webtoons on large screens**: art narrower than the strip (most webtoons on a computer or a phone) is enlarged with a sharper filter, with cleaner lines and text.
- **Pages sharpen after a resize or zoom**: widening the strip, resizing the window or zooming in decodes the page again at the new size, keeping the old picture until it is ready. Before, a widened strip stayed blurry.
- **Lighter on memory**: paged reading no longer decodes the pages it reads ahead only to learn their shape, and pages use far less video memory.
- **The image cache keeps what you are reading**: when it trims itself it now drops the images you looked at least recently, not the oldest downloads (pages you were still reading could go), and it writes to disk far less while you scroll.

## 2.3.5 — 2026-10-21

- **Extension anime plays again** ([#13](https://github.com/medium-gh/scancore/issues/13)): since 2.2.0 SCANCORE's player turned down most extension videos with "unrecognized file format". It now plays them.
- **Servers that disguise their videos play**: some servers (such as HD-1 on AnimeKai, Anikoto, AniWave and Anichi) send each part of a video dressed up as a picture. SCANCORE now unwraps them on the way to the player, on every platform, and downloads of them play offline.
- **AniZone loads** on desktop instead of failing with an error, and extensions that stream from a server of their own no longer stop about ten minutes into an episode.

## 2.3.4 — 2026-10-21

- **Swipe the anime spotlight**: swipe or drag sideways for the next or previous title, as in most apps. The slide follows your finger, a quick flick is enough, and a swipe that drifts a little no longer scrolls the page instead. (Swiping was meant to work before, but never changed the slide.)

## 2.3.3 — 2026-10-21

- **Reader controls stay out of the status bar**: on phones, the novel reader's top bar no longer slides up over the clock and icons when it hides as you scroll. Both readers now draw under the status bar and keep their controls below it.
- **The novel's title is readable on phones**: it gets its own row under the reader's buttons instead of shrinking to "…".

## 2.3.2 — 2026-10-21

- **Settings → Sources is fast again**: it lists SCANCORE's built-in sources (extensions have their own page), those you have series on first, a dozen at a time with Show all and a search. With many extensions installed it used to build a row for each of their sources.
- **Long lists elsewhere stay light**: the source chips on Comics and Novels, Anime's sources, Add a section, moving several series, Library health, chapter comments and reviews only build what is on screen or show the first ones with Show more.

## 2.3.1 — 2026-10-21

- **Read from opens faster**: a catalogue title is looked up on SCANCORE's built-in sources only, not on every installed extension, and each match shows as soon as its source answers. **My extensions** lists your extensions (recently used first, with a search) and looks on the one you pick; moving a series still asks everything, showing built-in matches first.
- **The Read from list scrolls** when it is long, and opens as a sheet from the bottom on phones.
- **Nothing spills out of its card on phones**: settings buttons beside long descriptions drop below them, wide choices stay inside, and the About, Activity and Sync buttons and tabs fit at 360 px.
- **No more crashes at startup** on systems whose Qt hashes through OpenSSL 3 (such as Arch Linux).
- **Your scancore.onl session stays yours**: the cookies of scancore.onl and your sync server are never shared with extensions, plugins or browser checks.
- The welcome tour says adding a repository is optional: the built-in sources work without one.

## 2.3.0 — 2026-10-21

SCANCORE 2.3 "One story": comic, novel and anime as one story, and a library that looks after itself.

- **The story elsewhere**: a series page shows its other forms side by side (the manhwa an anime adapts, the web novel behind it, every season) with how far you are in each, and opens them in one tap.
- **Where the anime leaves off**: anime pages say where the comic or novel goes on after that season ("Manhwa continues from chapter 46"), and comic and novel pages say where each season ends, with a button to go there. Readers suggest and vote on these on scancore.onl; SCANCORE's own list starts it off.
- **Continue the story** on Home: after you finish a season, or catch up on a comic whose novel is further ahead, a card shows where to go on.
- **Upcoming**: one calendar for what comes out next in your library, anime episodes from their schedule and comic and novel chapters guessed from each series' rhythm (Library → Upcoming).
- **Library health**: sources that stopped answering or updating are listed with a move to another source, and series saved twice on different sources can be merged, keeping everything you read in either. Library shows when something needs a look.
- **Five more trackers**: Kitsu, MangaUpdates and Bangumi (and Shikimori and Simkl in builds with their ids) in Settings → Connections.
- **Read a title from your extensions**: a search result or a series on a built-in source can be looked up on your installed extensions alone.
- **Home is yours to shape**: remove sections you do not want (they no longer load) and add shelves of an extension's latest or popular titles.
- Update checks can send one notification for everything they found (Settings → Library).
- The Flatpak builds again with SCANCORE's own player.
- Android: opening Sources no longer closes SCANCORE on phones without a working text-to-speech engine.

## 2.2.1 — 2026-10-21

- **Adding a second repository no longer closes SCANCORE.**
- **Repository links work however you paste them**: a GitHub repository's page or just owner/name, a raw folder, jsDelivr, Codeberg or GitLab pages, or the link to an index.min.json, repo.json or LNReader plugin list (such as raw.githubusercontent.com/LNReader/lnreader-plugins/…/plugins.min.json). Paste several at once and they are added one after another.
- **AniList connects inside SCANCORE**: sign in and allow it, and the account is connected; nothing to copy. When AniList, MyAnimeList or Comick signs SCANCORE out, you are told and can connect again in one tap.
- **Watching updates your trackers reliably**: an episode counts as watched from 85%, in its last two minutes, once the ending starts, or when you go on to the next one.
- **Continue watching and reading** show the episode or chapter you are really on, say which episode is next, and leave out series you only plan to watch, dropped or finished. Starting a planned series moves it to Watching or Reading.
- **Crash reports**: if SCANCORE closes unexpectedly, the next start offers a report to copy, share or post on GitHub. Settings → About → Crash reports and diagnostics has past reports and a log to send for other problems.

## 2.2.0 — 2026-10-21

SCANCORE 2.2 "Together": your devices and your TV, in step.

- **SCANCORE Wrapped**: your year in comics, novels and anime as a story across the whole window (time spent, chapters and episodes, the series you came back to most and your number one of each, the genres and author you read most, your busiest month and weekday, your longest streak, where your year started) and a card to save as a picture. Home shows it from 1 December to 7 January, and the year review opens it any time.
- **Live progress between your devices**: signed in to scancore.onl (or a SCANCORE sync server 1.1), where you are in a chapter or episode reaches your other open devices within seconds, not at the next sync. **Continue on another device** in the readers and the player sends your exact page or second to one of them, which offers to continue there. Settings → Sync shows which devices are open now.
- **Anime downloads**: download one episode, the next five or every unwatched one from an anime source (or the one playing) and watch without a connection, with its subtitles. Streams are kept as they are, up to 1080p, and a download that stopped goes on where it was.
- **Cast to a TV**: Chromecasts and smart TVs (DLNA) on your Wi-Fi. SCANCORE passes the TV what the video needs, so episodes from extensions and your downloads play there too; the player follows the TV with play, pause, seek and next and keeps your progress.
- **Open in another player**: mpv, VLC or IINA on computers, or MX Player, mpv, VLC and others on Android, with the video's headers, start position and subtitles.
- **Subtitle style** (size, colour, outline, a dark box behind the lines) and **Anime4K sharpening** for lower-resolution episodes, in Settings → Anime.
- **E-ink mode** for Boox, Kobo and other e-paper devices: black on white, no animation, comics in pages with a flash on page turns to clear ghosting, and novels turned page by page with a tap on the sides (Settings → Interface).
- **The Flatpak plays anime from extensions** with SCANCORE's own player, like the other packages, instead of only web players.
- **Four more interface languages**: Polski, ไทย, Filipino and हिन्दी (eighteen besides English).
- Bookmarks and each series' chapter-list settings (sort, filters) now sync between devices too.
- On phones, the player's bar has a menu for the new actions, and the readers' top bars fit on 360 px screens.

## 2.1.1 — 2026-10-21

- **Keiyoushi extensions work again.** Their current builds refused to start in SCANCORE ("CloudflareInterceptor must be present in default client") because SCANCORE's network setup was not exactly Mihon's; on desktop they also failed to load at all. Every one of them runs now, on Android and desktop.
- **Cloudflare checks pass by themselves**, as in Mihon: when a site shows a check, SCANCORE passes it in a browser you do not see and loads the page, usually in a few seconds. SCANCORE now introduces itself as the browser it really is, which is what lets most checks through without a click.
- When a site still wants you to tick the box, the check opens by itself while the page loads: one tap and it closes and the page carries on, nothing to try again. "Open browser check" appears on phones too, and every extension source has "Open the site" in Extensions (to pass a check or sign in), like Mihon's WebView.
- Extensions that open the site in a web view of their own (Comix) no longer wait minutes for a check they cannot see: the first visit to a site in a session passes it first.
- **Adding a repository** no longer has to install anything: "Add only" or "Add and install" the extensions in your language, which then install in the background while you use the app, with progress and a Stop button in Extensions.
- **Search your installed extensions** by name, source or site. Typing in a search box on phones (Extensions, History, a series' chapters) no longer closes the keyboard after every letter.
- On some Linux computers (Intel graphics with an NVIDIA laptop GPU) opening a Cloudflare check, or any page with 3D graphics, closed SCANCORE. It no longer does.

## 2.1.0 — 2026-10-20

SCANCORE 2.1 "Parity": what people coming from Mihon, Aniyomi and Kotatsu reach for first.

- **Move a series to another source**: "Move to another source" on a comic's or novel's page finds it on your other sources (under every name its catalogue knows), shows the match first (chapters, newest, how many will count as read) and moves it with its library list, favourite, tracker links, bookmarks and progress, carried by chapter number. Select series in the library to move them together, and Settings → Sources moves every series of a source at once, including sources that are gone. "Keep the old one too" copies instead.
- **Update rules**: choose which library lists are checked for new chapters, skip finished series, series with chapters left to read or series not started, and check by itself only on Wi-Fi and (Android) only while charging. New chapters of the lists you pick download by themselves, and "Delete chapters once read" clears a download when you finish the next chapter.
- **Chapter tools**: bookmarks, a filter for one translation group, unread, downloaded and bookmarked filters and a sort order remembered per series, "mark previous as read", and Select to download, mark, bookmark or remove many chapters at once.
- **Incognito mode and app lock** (Settings → Privacy): incognito keeps nothing of what you read or watch (no history, place, reading time, tracker updates or Discord status) and shows a badge while it is on. The app lock asks for a PIN when SCANCORE starts and after it has been away (right away, 1, 5 or 15 minutes); on Android fingerprint or face unlocks it too.
- **Comic reader**: a colour filter (brightness, warmth, greyscale, invert), wide spreads split into their two halves in the Pages layout, tap zones (left and right, L shape, Kindle, edges or off, flippable) shown on screen when you change them, volume keys that turn pages on Android, and page bar colours you choose (unread pages very light, read pages grey and the page you are on black to start). On phones the series and chapter title have their own row under the buttons, and the page count in the Pages layout no longer lags a page behind.
- **Backups**: Aniyomi backups restore onto the same anime extension exactly (episodes watched, the one part-way, bookmarks, list), or through AniList when the extension is not installed; LNReader backups restore novels onto the same plugin. Mihon backups now bring categories and bookmarks too.
- **Themes carry your whole look**: a theme file holds light or dark, accent, navigation, spacing, corners, text and cover sizes, motion, the pages you arranged and the readers' colours, and any theme can be exported, not only a custom one. Older theme files still open.
- **Phones**: dropdowns open as a sheet from the bottom, the first-run screen has a cleaner language picker and buttons, the library header takes four rows instead of seven, and Settings shows your scancore.onl profile photo.
- **Long novel chapters** open and scroll quickly: only the paragraphs near the screen exist (a 6000-paragraph chapter opens three times faster).
- **AVIF covers and pages** on Linux, in the Flatpak and on Android 12 and later.
- A repository's GitHub page (for example github.com/keiyoushi/extensions) can be added as an extension repository; SCANCORE reads the repository's own files from it.
- Taps no longer reach the tabs behind the first-run screen.

Still to come: the interface in Polish, Thai, Filipino and Hindi, SCANCORE's video player in the Flatpak, AVIF on Windows and macOS, and bookmarks and per-series settings in sync.

## 2.0.1 — 2026-10-20

- **"Read from" on Windows** found a series only under the names the catalogue gives it, never under the other names a source lists it by, so a series a source lists under its Russian, Spanish or Korean name was missed. It is found again, as on the other systems.
- **Kakao Page** for Korean comics: webtoons and manga with every episode it shows without an account (wait-for-free and paid episodes, and adult titles, need one and are left out). "Read from" opens a series there directly when AniList links to it.

## 2.0.0 — 2026-10-20

SCANCORE 2.0: comics, novels and anime from the extensions you already use, SCANCORE's own video player, pages you arrange yourself, the interface in fourteen languages, and sources in your language. Everything in 2.0.0-beta.1 below is in it, and since the beta:

- **Korean webtoons**: Naver Webtoon, with every episode it shows without an account (paid previews and adult titles, which need one, are left out).
- **Official WEBTOON in six more languages**: Spanish, French, German, Indonesian, Thai and Traditional Chinese, and Dongman Manhua in Simplified Chinese. They come with your content language like the other built-in sources.
- **Much more Spanish**: ZonaTMO, Olympus Scanlation and LectorManga, and ten scanlation groups' own sites (House Of Otakus, InfraFandub, Inventario Oculto, MHScans, Monopoly Scan, Ragnarok Scanlation, RichtoScan, Uchuujin Projects, UnderTranslations, Yuri-Online), each checked from search to the last page image.
- **"Read from" finds series under their translated names**: a source that lists a series as "Lector omnisciente" or "Torre de Dios" is now found from its English name, using the names the source and MangaDex know in your language. When AniList links a title to an official edition (WEBTOON in your language, Naver Webtoon), "Read from" opens that one directly. A spin-off that shares its series' name no longer stands in for the series.
- **MangaDex in other languages was always English** in "Read from": its series show under their name in that language, which never matched the catalogue's. They match now, and a title with no name in your language shows its English name instead of a romanised one.
- Chapter names SCANCORE makes up are in the source's language (Chapitre, Capítulo, Kapitel, Глава, 화, 話 and more).
- **Back up for Mihon** (Settings → Connections): saves the comics you read from extensions, with their categories and the chapters you read, as a .tachibk file Mihon or Tachiyomi restore. Restoring a Mihon backup puts each series on the same extension source when it is installed, as Mihon does.
- **Licences** (Settings → About): the open-source software inside SCANCORE and the full text of each licence.
- Built-in sources can be switched off and back on for everyone without an update, and are a separate part of SCANCORE's code; extensions and your files keep working either way.

Still to come: the interface in Polish, Thai, Filipino and Hindi, exact restores of Aniyomi and LNReader backups, and SCANCORE's video player in the Flatpak (the Flatpak plays anime in its web player). Kakao Webtoon and most Japanese publishers' sites protect their page images, so SCANCORE does not read them.

## 2.0.0-beta.1 — 2026-10-20

A test version of 2.0: comics, novels and anime from the extensions you already use, SCANCORE's own video player, pages you arrange yourself, ten more languages with sources in them, a library per media with categories, notifications, and a faster app everywhere. Export a backup before you install it (Settings → Storage → Back up everything).

- **Arrange every page**: Home, Comics, Novels and Anime have an Edit page button. Drag sections (or move them with arrows on phones), hide the ones you do not use, and give each shelf its own look: a row or a grid, small to large cards, how many items, and whether details and progress bars show. The search bar, source chips, greeting and anime spotlight move and hide too. Settings → Interface → Pages sets the spacing between sections and how round corners are, and resets everything.
- **SCANCORE in ten more languages**: Korean, Japanese, Chinese (simplified and traditional), Portuguese (Portugal), German, Italian, Russian, Turkish and Vietnamese, beside Spanish, French, Indonesian and Brazilian Portuguese.
- **Sources in your language**: built-in comic sources for Brazilian Portuguese, Indonesian, French, Turkish, Thai and Vietnamese, and MangaDex in Portuguese, Indonesian, French, Russian, Turkish, Vietnamese, Arabic, Thai, Korean, Chinese, German, Italian and Polish. SCANCORE shows the sources in your language (Settings → Sources → Source languages adds others), and when none of them has a series, "Read from" offers to look for it in another language.
- **Extensions for all three media**: add a comic, novel or anime extension repository from Settings or the first-run screen, or with one tap on a repository link (scancore://, tachiyomi://, mihon://, aniyomi:// and lnreader:// links open in SCANCORE). The new Extensions page installs, updates and removes extensions, asks before trusting a new signing key, and has each source's own settings. Comic and anime extensions run inside the app on Android; on desktop, SCANCORE downloads a small Java runtime for them the first time. Novel plugins run in SCANCORE itself on every system.
- **Browse any source**: its popular and latest lists and its own search filters, with covers and downloads through the extension. Search shows each source's results as soon as they arrive instead of waiting for the slowest, and "Read from" finds a series on your extension sources too, without being held up by a stuck one. Sites that ask for a browser check open in a browser window (on Android, the system's web view) and continue afterwards.
- **SCANCORE's own video player** for anime from extensions: hardware decoding, HLS streams, subtitles, switching servers without losing your place, the next episode starting by itself, resuming where you stopped, skipping openings and endings that the video marks, and Space, arrow and M keys. **Watch from** plays an anime from the AniList catalogue on one of your anime sources: SCANCORE picks the source when only one in your language has it, and asks otherwise.
- **Catalogues and trackers**: Comics, Novels and Anime each browse the catalogue you choose (AniList, MyAnimeList or Comick for comics; AniList or MyAnimeList for anime), chosen on first run or in Settings → Connections, where AniList, MyAnimeList and Comick connect too. A series page says when a tracker could not find the series and links it in one tap. "Read from" lists sources in your content language first.
- **Library per media**, each with Reading, Planned, Paused, Completed and Dropped, favourites, and a grid, compact or list view.
- **Notifications**: new chapters, extension and app updates and announcements from scancore.onl are kept in a notification area with an unread count, and reach the system's notifications when SCANCORE is not on screen.
- **Full-screen reading** that is remembered, without the status bar or the page bar.
- **The comic reader's page bar your way**: always shown, only with the reader's controls (tap the page), or off, in thin, medium or thick (the reader's settings menu). Drag along it to move through the pages.
- **Anime on Android start with sound and go full screen**: episodes from the built-in servers used to start muted, and the player's full screen button said full screen was not supported. It now turns the phone to landscape without the system bars, and Back leaves it. The full screen button above the player turns the phone too, even with auto-rotate off.
- "Add to library" on an anime saves it and changes to "In library" at once; it said "Removed from your library" and stayed as it was. The library button, favourites and progress on comic and novel pages, and lists of sources and extensions, also update as soon as they change instead of when the page is opened again.
- **Novels**: read aloud with your system's voices, and much more control over the page: fonts, weight, line and paragraph spacing, margins, indent, your own colours, dimming and auto-scroll. A built-in source for a machine-translated novel site (English).
- **Faster**: SCANCORE starts about twice as fast, covers are drawn by one shader and grids create only what is on screen, sections no longer freeze the window the first time they open, and fitted comic pages shrink cleanly. Mouse wheels and touchpads scroll the same way everywhere, with a speed setting in Settings → Interface.
- Covers and pages from servers that sent AVIF images show again: SCANCORE now asks only for formats it can show.
- MangaDex says when every chapter of a series was taken down instead of showing an empty list.
- Continue reading shows a series once, from the source you read it on last, and a saved cover that a site moved is fetched again. Episodes of one anime airing at the same time are one entry in the schedule (Ep 1–10).
- Android: SCANCORE keeps running in the background for downloads, update checks and notifications, and offers to turn off battery restrictions for it.

Not in this beta yet: Flatpak and package-manager builds (use the AppImage on Linux), restoring Mihon, Aniyomi and LNReader backups exactly, the licences screen, and the interface in Polish, Thai, Filipino and Hindi. The video player has been tested on Linux and in the Android emulator so far; on Windows, macOS and phones it is new in this beta.

## 1.5.0 — 2026-10-19

Spanish comic sources, source languages, accent-proof title matching, episodes that open again from an anime's page, and search from the Android keyboard.

- **Comics in Spanish**: five new sources. MangaDex (Español) lists Spanish and Latin American Spanish translations, with Spanish titles and descriptions where MangaDex has them; ManhwaWeb, InManga, LeerCapitulo and MangaOni are Spanish sites. A series read in Spanish is its own library entry, separate from the same series in English. InManga has lost the pages of many older chapters, and SCANCORE says so when you open one.
- **Source languages**: Settings → Sources shows each source's language, and comic source chips show it when it is not English. A new setting keeps "All sources" (and "Read from" for Comick's catalogue) to one language; it searches every language unless you change it.
- **Language on the first screen**: the welcome screen starts with the interface language, so the rest of the intro is in it, and its look step can pick the comic source language. Its Theme, Accent, Comics open as and Reduce motion labels are translated now.
- **Titles match with or without accents**: "Pequeña" and "pequena" are the same title in search, "Read from" and AniList, MyAnimeList and Comick matching. Japanese, Korean and Chinese titles take part in comic search ranking too; they were ignored before.
- **Tracking by MangaDex's links**: comics from MangaDex (either language) are matched on AniList and MyAnimeList by the ids MangaDex gives, not only by title.
- **Spanish chapter numbers**: "Capítulo 12", "Cap. 12" and "Episodio 12" are read as chapter 12, and "Temporada 2 Capítulo 12" no longer counts as chapter 2. Chapter names SCANCORE makes up are in the source's language.
- Tapping or clicking an episode on an anime's page plays it again; it did nothing since 1.4.0. The play button on episode thumbnails is white again instead of a black disc.
- Sites behind Cloudflare that work normally are no longer reported as asking for a browser check.
- Android: pressing Enter on the keyboard in a search field searches, instead of moving to the clear button.
- Android: the comic reader's top bar sits right under the status bar instead of a gap below it.

## 1.4.0 — 2026-10-19

Comments and reviews from scancore.onl in the app, comics from Comick's catalogue, chapter lists ten at a time, the page bar back at the reader's edge, and access codes for new scancore.onl accounts.

- **Comments and reviews in the app**: comics, novels and anime have Reviews and Discussion tabs beside their chapters or episodes, and the readers and the player have a comments button for the chapter or episode you are on. They are the same threads as on scancore.onl, so a comment posted on the website shows in the app and the other way round. Anyone can read them; sign in to your scancore.onl account in Settings → Sync to post, reply, vote and rate.
- **Comics keyed on Comick's catalogue**: the Comics page shows Comick's trending, most popular, new and recently completed comics, and a comic opened from the catalogue keeps its catalogue entry. That ties it to the same comments and reviews whichever source you read it on, and Comick tracking uses it instead of guessing by title. "Read from" also looks for a comic under every name the catalogue knows, so entries from the shelves are found on more sources.
- **Chapter lists ten at a time**: a series page shows ten chapters per page with page numbers and a chapter search (on phones too, where the list used to be one long column), and opens on the page of the chapter you are on.
- **The comic reader's page bar is back at the left edge**: one thin segment per page (read pages dark, unread grey, the page you are on white) that you can tap to jump to a page. The bottom bar with the page slider is gone; the page number is under the chapter title and the previous and next chapter buttons are at the top.
- **Creating a scancore.onl account needs an access code**: the sign-up form shows six boxes for it when the site asks for one ("Contact logesh@mediumdev.com for access"). Existing accounts, signing in, your own SCANCORE sync server, WebDAV and folder sync are unchanged.

## 1.3.0 — 2026-10-18

Activity stats, self-hosted/WebDAV/folder sync, local comics and EPUBs, Comick-powered search, AniList/MAL/Comick tracking, library imports, two-page and zoom in the comic reader, theme gallery, four new languages, and many fixes.

- **Activity**: a new page (Ctrl+8) with the time you spent reading and watching, chapters and episodes finished, your streak, a calendar of active days coloured by what you did, and charts for the last 30 days, 12 weeks or 12 months, on every device you sync. **Your year** sums it up on one card you can save as a picture.
- **Sync without scancore.onl**: Settings → Sync can use a SCANCORE sync server on your own computer, NAS or server ([scancore-web](https://github.com/medium-gh/scancore-web), one Docker command), any WebDAV folder (Nextcloud, ownCloud, a NAS) or a folder that Syncthing, Dropbox, OneDrive or iCloud Drive keeps in step. Each device keeps its own small file and reads the others', so the library, removals, progress and activity follow you without an account, and Settings lists the devices that sync together.
- **Comics and books on your device**: choose folders in Settings → Sources and Comics and Novels list them under On this device: folders of images, CBZ, CBR and 7z archives and PDF files as comics, and EPUB books as novels. They open from your file manager too.
- **Search almost every series**: the comic search asks Comick's catalogue, which knows nearly every manga, manhwa and manhua under all its names, and opens your pick from whichever of your sources has it (it asks which one when several do). Settings → Sources turns this off to search each source directly, as before.
- **AniList, MyAnimeList and Comick** (Settings → Connections): finishing a chapter or episode moves the series forward on your lists there. Anime are matched by their AniList entry and comics and novels by title, and any series can be linked by hand from the new tracking button on its page.
- **Bring your library**: import a Mihon or Tachiyomi backup, a public AniList list, a MyAnimeList export or your Comick library. Anime come in with the episodes you watched, comics with the chapters you read when one of your sources has them, and the rest wait in a list that SCANCORE can search every source for.
- Comic reader, in page mode: **two pages side by side** in wide windows (covers and double-page spreads stay alone), **trimming of white or black page borders**, and **zoom** by pinching, double-clicking the middle of the page or Ctrl + scroll; drag to look around a zoomed page. **Each series can keep its own reader settings** (layout, direction, width, spreads, trimming) from the reader's settings.
- **Theme gallery** in Settings → Theme: eight themes to start from, and anyone can share theme files through the scancore repository. Opening a .sct file from your file manager now opens it in SCANCORE, and SCANCORE appears among the apps that open CBZ, CBR and EPUB files.
- **SCANCORE speaks Spanish, French, Brazilian Portuguese and Indonesian**, following your system or chosen in Settings → Interface → Language, which switches without a restart.
- Account sync no longer starts a second sync a minute after every sync.
- **Fixed parts of SCANCORE working on separate copies of the library, account, downloads and updater**, a mix-up present since 1.0. Automatic update checks now show the update banner (only a check from Settings → About did before), saving or removing a series starts a sync and removals reach scancore.onl, signed-in users no longer sync twice at startup, and changes in Settings, such as how often to check for new chapters or app updates, apply without a restart.
- Opening SCANCORE while it is already running, for example in the tray, brings its window back instead of starting a second copy on the same library.
- Reading keys are yours to change as well: scrolling by a line or a screen (↓ ↑, J K, Space, Page Up/Down) is in Settings → Keyboard, and the novel reader now follows all your reader shortcuts; it ignored changed keys before.
- The novel reader goes back to where you stopped in a chapter that was loaded ahead (it started at the top), counts a chapter that fits on one screen as read, and its menus and messages fit narrow phones.
- The Ctrl+K palette groups its results under Search, Recent, Pages and Commands, and pointing at or clicking a result works again.
- The anime player remembers the server you picked and starts the next episode on it.
- Release pages list what changed and which file to download for each system.

## 1.2.0 — 2026-10-18

- **Your own theme.** Pick the window colour on a colour wheel, with a brightness slider that decides how dark it is, and the whole app follows it. Add a picture or animated GIF behind everything, fitted, dimmed and blurred to taste. Panels can be see-through, and so can the whole window, showing your desktop through it. Accents can be any colour too, and indigo, emerald and amber join the presets.
- **Share themes** as .sct files: one file with the colours, transparency and background picture inside it, which works on every system. Import from Settings → Theme, or drop the file on the window.
- **Settings shows one category at a time** on desktop, with a search box over the list of categories, instead of one long page to scroll through.
- **Navigation your way**: a full sidebar, a floating one, an icon rail or a dock, on the left or right (the dock on the top or bottom).
- **Change any shortcut** in Settings → Keyboard: click it and press the new keys. A key taken from another action moves over, and each one can go back to its default. The shortcuts sheet (**?**) shows yours.
- Settings → Interface adds text size, cover size (fit more covers on screen, or fewer and larger), animation speed (relaxed, default or snappy) and the page SCANCORE opens on.
- **Pick up where you left off.** Home leads with the last thing you read or watched, with its progress and a Continue button.
- **Library sorting**: new chapters first, recently read, recently added, or by title. History has a search box.
- **Recent searches** on the Comics and Novels pages, one click to search again.
- **Quick commands** in the Ctrl+K palette: switch theme, reduce motion, check for new chapters, sync, and a sheet of every keyboard shortcut (also on **?**).
- Comic reader: in page mode, swipe or drag sideways to turn the page.
- Anime: endings can be skipped automatically too, and the next-episode countdown can be 3, 8 or 15 seconds. With intros skipped automatically, the Skip button now still appears for endings, which were otherwise impossible to skip.
- Livelier details: notifications show an icon and the time left, pause while you point at them and can be swiped away; the anime spotlight shows when the next title comes, pauses on hover and can be swiped; shelves fade at the edge that scrolls; long pages have a back-to-top button; dialogs spring in.
- Lists no longer blink or replay their entrance when the library changes in the background, for example after a sync or while chapters download, and the Downloads list keeps its place during downloads.
- Signing out asks first.
- The sidebar scrolls instead of overlapping its Settings button in short windows.
- The logo on Settings → About is no longer stretched.
- **Anime plays on Windows, macOS and Linux again.** Episodes are H.264 video, which the browser built into the desktop downloads could not decode, so the player only ever showed a spinner (Android was unaffected). Windows now plays through Microsoft Edge WebView2 and macOS through the system's WebKit. The Flatpak gains the player, using Flathub's Qt WebEngine, which has the codec, and the AppImage is built with a Qt WebEngine that has it too. Where a build still cannot play a video, the player says so and offers to open the episode in your browser.
- The player has a button to open the episode in your browser. On Windows and macOS the episode list sits beside the video, and in full screen the controls come back when the pointer reaches the top edge.
- **Sync is lighter.** While you watch or read, changes are sent to scancore.onl at most once a minute instead of every few seconds, and saves the website does not recognise are no longer offered again at every sync.
- Series you remove from your library are removed on scancore.onl too, instead of coming back at the next sync.
- A sync started from Settings can no longer stop halfway and leave "Syncing…" showing until a restart.
- Settings → Account no longer lists what syncs.
- Phones: page titles keep their full width (their buttons move below them), download progress shows its text again, and the anime schedule's day picker, search field and long titles fit the screen. Dialogs and reader messages fit narrow screens.
- Anime Browse shows a Load more button when the first results do not fill the page.
- The mouse wheel scrolls the page again with the pointer over a row of covers (Home, Anime, Novels and more); the row kept the wheel for itself, most noticeably on Windows. Shift with the wheel, or a sideways wheel, still scrolls the row.
- Comic pages from sites that protect their images (Webtoon among them) could stay black when a chapter opened, the first page in page mode most of all: the page was requested before the reader knew which site it came from. They load now, and a page that fails in page mode says so and offers Retry, as the vertical strip does.
- The Downloads page no longer measures the whole downloads folder on every page that downloads, which made it stutter during large downloads.

## 1.1.0 — 2026-10-17

- **Back works again.** Esc, the close arrow and Android Back now leave the comic reader, novel reader, anime player and browser checks; they did nothing before. Esc also goes back from details pages, and the back button on mice works.
- **Comic reader controls stay out of the way.** They no longer reappear on their own while you read or scroll; move the mouse (desktop) or tap (phone) to bring them back. On phones the readers now fill the screen behind the status bar, and the comic reader's top bar sits at the very top.
- **Anime progress syncs.** With a scancore.onl account, episodes you watch and where you stopped now sync between your devices and the website.
- **New Settings.** Settings is organised into categories with picture choices for the theme and reader layout, a live preview for novel text, and its own phone layout. Panel gap, progress rail, line spacing, column width and justified text are now adjustable there.
- **Faster to pick up where you left off.** Comics and novels you have opened before show their chapters immediately, and Continue starts reading without waiting for the site. A request that fails because the connection blinked is retried once automatically.
- Android: Back on Home now leaves the app (it stays running in the background), and a tap shows or hides the reader controls.
- Buttons and chips on the accent colour have readable text in the light theme and with coloured accents.
- Phones: source pickers scroll sideways instead of wrapping, the library filter no longer runs off screen, and reader titles show.
- Leaving the phone player in landscape no longer leaves the app in full screen (thanks to @Doomguy-BFG9000).
- Building from source now requires Qt 6.9.

## 1.0.0 — 2026-10-16

- First release in the reset v1 series. This release retains the current SCANCORE code, including the desktop and phone interface improvements.
- Read comics and novels, browse and watch anime, keep a local library and reading history, and download chapters for offline use.
- Linux packaging supports Flatpak-managed updates and can build without the optional downloadable Python novel-source pack.
- Android's numeric build code remains above previously shipped builds so existing Android installations can upgrade despite the public version reset.
