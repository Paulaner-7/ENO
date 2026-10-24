package com.mediumdev.scancore.host.desktop

import android.app.Application
import android.os.Handler
import android.os.Looper
import android.webkit.JavascriptInterface
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import com.sun.net.httpserver.HttpServer
import org.junit.AfterClass
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.BeforeClass
import org.junit.Test
import java.io.File
import java.lang.reflect.InvocationHandler
import java.lang.reflect.Proxy
import java.net.InetSocketAddress
import java.nio.file.Files
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

// The web view extensions open on computers (Chromium through JCEF), driven
// the way Keiyoushi's runWebView drives it: load a page, read values with
// evaluateJavascript (JSON, as Android gives them), hear from the page
// through addJavascriptInterface, and answer one of its requests from
// shouldInterceptRequest. Needs JetBrains Runtime's JCEF, so it runs only
// with SCANCORE_TEST_JCEF set to a jbr_jcef-*.tar.gz (CefRuntime's JBR
// release for this system); without, it is skipped.
class WebViewTest {
    // Keiyoushi's copies of androidx.webkit's boundary interfaces, as far as
    // it uses them (its own classes; only the method names matter).
    interface FactoryBoundary {
        fun getSupportedFeatures(): Array<String>

        fun createWebView(view: WebView): InvocationHandler
    }

    interface ProviderBoundary {
        fun addDocumentStartJavaScript(
            script: String,
            rules: Array<String>,
        ): InvocationHandler
    }

    companion object {
        private lateinit var server: HttpServer
        // About 600 MB each: removed afterwards.
        private val made = mutableListOf<File>()
        private val archive = System.getenv("SCANCORE_TEST_JCEF")?.let(::File)

        @BeforeClass
        @JvmStatic
        fun start() {
            if (archive == null) return
            if (System.getProperty("scancore.host.root") == null) {
                setUp(Files.createTempDirectory("scancore-webview-test").toFile().also { made += it }, null, "test")
            }
            // As after a download: the archive in the work folder.
            val work = Files.createTempDirectory("scancore-webview-unpack").toFile().also { made += it }
            CefRuntime.install(archive.copyTo(File(work, archive.name)), work)
            server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)
            server.createContext("/page") { exchange ->
                val body =
                    """
                    <!doctype html><title>Test page</title>
                    <script>
                    window.answer = 6 * 7;
                    bridge.post("loaded:" + document.title);
                    bridge.post("early:" + window.early);
                    addEventListener("touchstart", e => bridge.post("touch:" + e.isTrusted + ":" + Math.round(e.touches[0].clientX)));
                    fetch("/data.json").then(r => r.text()).then(t => bridge.post("data:" + t));
                    </script>
                    """.trimIndent().toByteArray()
                exchange.responseHeaders.add("Content-Type", "text/html")
                exchange.sendResponseHeaders(200, body.size.toLong())
                exchange.responseBody.use { it.write(body) }
            }
            server.createContext("/data.json") { exchange ->
                val body = """{"from":"site"}""".toByteArray()
                exchange.sendResponseHeaders(200, body.size.toLong())
                exchange.responseBody.use { it.write(body) }
            }
            server.start()
        }

        @AfterClass
        @JvmStatic
        fun stop() {
            if (archive != null) server.stop(0)
            made.forEach { it.deleteRecursively() }
            // Where CefRuntime put the browser (another test's root).
            System.getProperty("scancore.host.root")?.let { File(it, "web-view").deleteRecursively() }
        }
    }

    @Test
    fun runsPagesAsKeiyoushiDoes() {
        assumeTrue("SCANCORE_TEST_JCEF is not set", archive != null)
        val base = "http://127.0.0.1:${server.address.port}"
        val messages = CopyOnWriteArrayList<String>()
        val evaluated = CopyOnWriteArrayList<String>()
        val intercepted = CopyOnWriteArrayList<String>()
        val done = CountDownLatch(5)
        var view: WebView? = null
        Handler(Looper.getMainLooper()).post {
            view =
                WebView(Application()).apply {
                    settings.javaScriptEnabled = true
                    addJavascriptInterface(
                        object {
                            @JavascriptInterface
                            fun post(message: String) {
                                messages += message
                                done.countDown()
                            }
                        },
                        "bridge",
                    )
                    webViewClient =
                        object : WebViewClient() {
                            override fun onPageFinished(
                                view: WebView,
                                url: String,
                            ) {
                                view.evaluateJavascript("window.answer") {
                                    evaluated += it
                                    done.countDown()
                                }
                                // A finger, as Comix sends one.
                                val down = android.os.SystemClock.uptimeMillis()
                                for (action in listOf(android.view.MotionEvent.ACTION_DOWN, android.view.MotionEvent.ACTION_UP)) {
                                    val event = android.view.MotionEvent.obtain(down, android.os.SystemClock.uptimeMillis(), action, 100f, 50f, 0)
                                    view.dispatchTouchEvent(event)
                                    event.recycle()
                                }
                            }

                            override fun shouldInterceptRequest(
                                view: WebView,
                                request: WebResourceRequest,
                            ): WebResourceResponse? {
                                val url = request.url.toString()
                                if (!url.endsWith("/data.json")) return null
                                intercepted += url
                                return WebResourceResponse("application/json", "utf-8", """{"from":"host"}""".byteInputStream())
                            }
                        }
                    // As Keiyoushi's WebViewGlueBridge finds the glue.
                    val loader =
                        WebView::class.java
                            .getDeclaredMethod("getFactory")
                            .apply { isAccessible = true }
                            .invoke(null)
                            .javaClass.classLoader
                    val glue = Class.forName("org.chromium.support_lib_glue.SupportLibReflectionUtil", false, loader)
                    val factory =
                        Proxy.newProxyInstance(
                            javaClass.classLoader,
                            arrayOf(FactoryBoundary::class.java),
                            glue.getDeclaredMethod("createWebViewProviderFactory").invoke(null) as InvocationHandler,
                        ) as FactoryBoundary
                    assertTrue("DOCUMENT_START_SCRIPT:1" in factory.getSupportedFeatures())
                    val provider =
                        Proxy.newProxyInstance(javaClass.classLoader, arrayOf(ProviderBoundary::class.java), factory.createWebView(this))
                            as ProviderBoundary
                    provider.addDocumentStartJavaScript("window.early = typeof bridge + ':' + document.readyState", arrayOf(base))
                    loadUrl("$base/page")
                }
        }
        val finished = done.await(90, TimeUnit.SECONDS)
        Handler(Looper.getMainLooper()).post { view?.destroy() }
        assertTrue("timed out: messages $messages, evaluated $evaluated", finished)
        assertTrue(messages.toString(), "loaded:Test page" in messages)
        assertTrue(messages.toString(), """data:{"from":"host"}""" in messages)
        assertEquals("42", evaluated.first())
        assertTrue(messages.toString(), "early:object:loading" in messages)
        assertTrue(messages.toString(), "touch:true:100" in messages)
        assertEquals(1, intercepted.size)
    }
}
