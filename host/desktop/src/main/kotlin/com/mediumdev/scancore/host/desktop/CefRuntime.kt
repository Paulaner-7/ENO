package com.mediumdev.scancore.host.desktop

import android.webkit.WebView
import android.webkit.WebViewProvider
import com.jetbrains.cef.JCefAppConfig
import eu.kanade.tachiyomi.network.HostNetwork
import eu.kanade.tachiyomi.network.MemoryCookieJar
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import okhttp3.OkHttpClient
import okhttp3.Request
import org.cef.CefApp
import org.cef.CefSettings
import org.cef.SystemBootstrap
import org.cef.network.CefCookie
import org.cef.network.CefCookieManager
import xyz.nulldev.androidcompat.webkit.CefHelper
import xyz.nulldev.androidcompat.webkit.KcefWebViewProvider
import xyz.nulldev.androidcompat.webkit.NoWebViewProvider
import java.io.File
import java.security.MessageDigest
import java.util.Date
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

// The web view extensions open (Keiyoushi's runWebView: Comix and others)
// on computers: Chromium through JetBrains' JCEF, as Suwayomi does. Its
// native part (about 250 MB) comes with a JetBrains Runtime release and is
// downloaded the first time a source asks for a web view, checked against
// JetBrains' published SHA-512, and kept in <root>/web-view. Until it is
// ready, a source that needs it says so and works once it is.
object CefRuntime {
    // The release whose native JCEF matches the jcef jar (libs.versions.toml).
    private const val JBR = "25.0.4"
    private const val BUILD = "b508.27"

    private val dir by lazy { File(System.getProperty("scancore.host.root"), "web-view") }
    private val marker get() = File(dir, "scancore-web-view.txt")

    private sealed interface State {
        data object Idle : State

        data class Downloading(
            val done: Long,
            val total: Long,
        ) : State

        data object Starting : State

        data object Ready : State

        data class Failed(
            val message: String,
        ) : State
    }

    @Volatile
    private var state: State = State.Idle
    private val lock = Any()

    // What WebView(context) gets: the real web view once it runs, until then
    // one that explains (and the download starts).
    fun providerFor(view: WebView): WebViewProvider {
        if (installed()) {
            start()
            if (state == State.Ready) return KcefWebViewProvider(view)
        } else {
            download()
        }
        return NoWebViewProvider.create(view, message())
    }

    private fun message(): String =
        when (val s = state) {
            is State.Downloading ->
                "This source opens its site in a web browser, which SCANCORE is downloading now " +
                    "(${percent(s)}, about ${s.total / 1_000_000} MB, only once). Try again when it is done."
            is State.Failed -> "This source needs a web browser, which could not be set up: ${s.message}"
            else -> "This source needs a web browser, which SCANCORE is getting ready. Try again in a minute."
        }

    private fun percent(s: State.Downloading) = if (s.total > 0) "${s.done * 100 / s.total}%" else "starting"

    private fun installed() = marker.isFile && marker.readText().trim() == "$JBR-$BUILD"

    private fun platform(): Pair<String, String> {
        val os = System.getProperty("os.name").lowercase()
        val arch = System.getProperty("os.arch").lowercase().let { if (it == "aarch64" || it == "arm64") "aarch64" else "x64" }
        return when {
            os.startsWith("windows") -> "windows" to arch
            os.startsWith("mac") -> "osx" to arch
            else -> "linux" to arch
        }
    }

    // In the background; providerFor says how far it got.
    private fun download() {
        synchronized(lock) {
            if (state is State.Downloading || state == State.Starting) return
            state = State.Downloading(0, 0)
        }
        thread(name = "web-view-download", isDaemon = true) {
            state =
                try {
                    fetch()
                    State.Idle
                } catch (e: Throwable) {
                    System.err.println("Web view download failed: $e")
                    State.Failed(e.message ?: e.toString())
                }
        }
    }

    private fun fetch() {
        val (os, arch) = platform()
        val name = "jbr_jcef-$JBR-$os-$arch-$BUILD.tar.gz"
        val url = "https://cache-redirector.jetbrains.com/intellij-jbr/$name"
        val client =
            OkHttpClient
                .Builder()
                .followRedirects(true)
                .readTimeout(60, TimeUnit.SECONDS)
                .build()
        val expected =
            client.newCall(Request.Builder().url("$url.checksum").build()).execute().use {
                check(it.isSuccessful) { "No checksum for $name (HTTP ${it.code})" }
                it.body.string().trim().substringBefore(' ').lowercase()
            }
        val work = File(dir.parentFile, "web-view.part").apply { deleteRecursively(); mkdirs() }
        val archive = File(work, name)
        val digest = MessageDigest.getInstance("SHA-512")
        client.newCall(Request.Builder().url(url).build()).execute().use { response ->
            check(response.isSuccessful) { "The download failed (HTTP ${response.code})" }
            val total = response.body.contentLength()
            var done = 0L
            response.body.byteStream().use { input ->
                archive.outputStream().buffered().use { output ->
                    val buffer = ByteArray(1 shl 16)
                    while (true) {
                        val n = input.read(buffer)
                        if (n < 0) break
                        output.write(buffer, 0, n)
                        digest.update(buffer, 0, n)
                        done += n
                        state = State.Downloading(done, total)
                    }
                }
            }
        }
        val actual = digest.digest().joinToString("") { "%02x".format(it) }
        check(actual == expected) { "The download was damaged (checksum differs)" }
        install(archive, work)
    }

    // A JetBrains Runtime archive with JCEF, unpacked into place (tests pass
    // one already downloaded).
    internal fun install(
        archive: File,
        work: File = File(dir.parentFile, "web-view.part").apply { deleteRecursively(); mkdirs() },
    ) {
        val (os, _) = platform()
        // tar is part of Windows 10 and later, macOS and Linux.
        val unpacked = File(work, "unpacked").apply { mkdirs() }
        val tar = ProcessBuilder("tar", "-xzf", archive.absolutePath, "-C", unpacked.absolutePath).redirectErrorStream(true).start()
        val log = tar.inputStream.bufferedReader().readText()
        check(tar.waitFor() == 0) { "Unpacking failed: ${log.take(300)}" }
        if (archive.parentFile == work) archive.delete()
        val top = unpacked.listFiles()!!.single { it.isDirectory }
        dir.deleteRecursively()
        dir.mkdirs()
        // What JCefAppConfig.getInstance(dir) reads: Linux and Windows the
        // runtime's lib (bin on Windows) folder; macOS lib and Frameworks.
        when (os) {
            "osx" -> {
                val contents = File(top, "Contents")
                File(contents, "Home/lib").listFiles()!!.forEach { it.renameTo(File(dir, it.name)) }
                File(contents, "Frameworks").renameTo(File(dir, "Frameworks"))
            }
            "windows" -> {
                File(top, "lib").listFiles()!!.forEach { it.renameTo(File(dir, it.name)) }
                File(top, "bin").listFiles()!!.forEach { it.renameTo(File(dir, it.name)) }
            }
            else -> File(top, "lib").listFiles()!!.forEach { it.renameTo(File(dir, it.name)) }
        }
        work.deleteRecursively()
        marker.writeText("$JBR-$BUILD\n")
    }

    // Once per process (CefApp cannot start twice); a few seconds.
    private fun start() {
        synchronized(lock) {
            if (state == State.Ready || state is State.Failed) return
            state = State.Starting
            state =
                try {
                    initialize()
                    State.Ready
                } catch (e: Throwable) {
                    System.err.println("Web view failed to start: $e")
                    State.Failed(e.message ?: e.toString())
                }
        }
    }

    private fun initialize() {
        System.loadLibrary("jawt")
        val app =
            CefApp.getInstanceIfAny() ?: run {
                val config =
                    JCefAppConfig.getInstance(dir.absolutePath, false).apply {
                        appArgsAsList.addAll(
                            listOf(
                                "--disable-gpu",
                                // Drawn into memory, never into a window.
                                "--off-screen-rendering-enabled",
                                "--disable-dev-shm-usage",
                                "--change-stack-guard-on-fork=disable",
                                "--mute-audio",
                            ),
                        )
                        cefSettings.apply {
                            windowless_rendering_enabled = true
                            cache_path = File(dir.parentFile, "web-view-cache").absolutePath
                            log_severity = CefSettings.LogSeverity.LOGSEVERITY_WARNING
                        }
                    }
                // As CefApp does inside, but here, so a failure is an
                // exception rather than a browser that never comes.
                val os = System.getProperty("os.name").lowercase()
                when {
                    os.startsWith("windows") -> {
                        config.getLoader().loadLibrary("chrome_elf")
                        config.getLoader().loadLibrary("libcef")
                    }
                    os.startsWith("mac") -> {}
                    else -> config.getLoader().loadLibrary("cef")
                }
                config.getLoader().loadLibrary("jcef")
                CefApp.setIsRemoteEnabled(config.isRemoteEnabled)
                SystemBootstrap.setLoader(config.getLoader())
                CefApp.startup(config.appArgs)
                CefApp.getInstance(config.appArgs, config.cefSettings, config.serverExe)
            }
        CefHelper.cefApp.value = Result.success(app)
        runBlocking { withTimeout(60_000) { CefHelper.waitForInit().first() } }
        // The host's cookies (browser checks passed in SCANCORE, sign-ins)
        // in each new web view, as Suwayomi does.
        KcefWebViewProvider.initHandler =
            object : KcefWebViewProvider.InitBrowserHandler {
                override fun init(provider: KcefWebViewProvider) {
                    val manager = CefCookieManager.getGlobalManager() ?: return
                    for (cookie in (HostNetwork.cookieJar as? MemoryCookieJar)?.all().orEmpty()) {
                        val expires = if (cookie.persistent) Date(cookie.expiresAt) else null
                        manager.setCookie(
                            (if (cookie.secure) "https://" else "http://") + cookie.domain,
                            CefCookie(
                                cookie.name, cookie.value, cookie.domain, cookie.path, cookie.secure, cookie.httpOnly,
                                Date(), Date(), expires != null, expires,
                            ),
                        )
                    }
                }
            }
        Runtime.getRuntime().addShutdownHook(Thread { runCatching { CefApp.getInstanceIfAny()?.dispose() } })
    }
}
