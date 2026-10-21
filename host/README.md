# SCANCORE extension host

Runs Mihon extensions (comics) and Aniyomi extensions (anime) for the SCANCORE app.

| Module | What |
| --- | --- |
| `api` | The extension API: Mihon's source API and network helpers, Aniyomi's anime source API, with SCANCORE's NetworkHelper. Extensions are compiled against these names. |
| `compat` | Android API stand-ins for the JVM (Suwayomi's AndroidCompat) and AOSP's android.jar stubs. |
| `core` | The protocol (one JSON object per line on stdin/stdout, calls answered in any order) and the calls: popular, latest, search, filters, details, chapters, pages, fetchImage, preferences; for anime also episodes, seasons, hosters, hosterVideos, resolveVideo. |
| `desktop` | The helper process: verifies an APK's signature, converts it with dex2jar (once per version, cached), loads its sources. |
| `android` | The host inside the Android app (an AAR in the APK, spoken to over JNI): loads extensions as Mihon does, with PackageManager and a DelegateLastClassLoader, no conversion. Built when an Android SDK is found. |
| `testext` | Tiny test extensions (a Mihon one and an Aniyomi one), packaged as signed APKs with the Android SDK's tools. |

CMake builds it (target `scancore_host`) when a JDK 17+ is installed;
`ctest` runs its tests (`host`) and the app-side end-to-end test
(`host_process`). On its own: `./gradlew :desktop:installDist :desktop:test`.

Android builds of the app build `:android:appBundle` and package the AAR
with the libraries it lists (packaging/android/build.gradle). Its tests run
on a device or emulator: `./gradlew :android:connectedDebugAndroidTest`.

See NOTICE.md for the code from other projects and their licences.
