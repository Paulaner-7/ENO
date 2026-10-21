package com.mediumdev.scancore.host.desktop

import com.sun.net.httpserver.HttpServer
import eu.kanade.tachiyomi.network.BrowserCheckException
import eu.kanade.tachiyomi.network.interceptor.CloudflareInterceptor
import eu.kanade.tachiyomi.network.BrowserCheckSolver
import eu.kanade.tachiyomi.network.HostNetwork
import eu.kanade.tachiyomi.network.MemoryCookieJar
import okhttp3.Cookie
import okhttp3.OkHttpClient
import okhttp3.Request
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import java.net.InetSocketAddress
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

// A site behind a Cloudflare check: the hidden check passes it and the
// request goes again by itself; parallel requests wait for one solve; a
// check that cannot be passed goes to the reader with the extension's user
// agent.
class BrowserCheckTest {
    private lateinit var server: HttpServer
    private val solves = AtomicInteger()
    private val savedJar = HostNetwork.cookieJar
    private val savedSolver = HostNetwork.browserCheckSolver
    private val base get() = "http://127.0.0.1:${server.address.port}"

    @Before
    fun start() {
        HostNetwork.cookieJar = MemoryCookieJar()
        server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)
        server.executor = Executors.newFixedThreadPool(8)
        server.createContext("/") { exchange ->
            val cleared = exchange.requestHeaders.getFirst("Cookie").orEmpty().contains("cf_clearance=good")
            val body = (if (cleared) "the page" else "<title>Just a moment...</title>").toByteArray()
            exchange.responseHeaders.add("Server", "cloudflare")
            if (!cleared) exchange.responseHeaders.add("cf-mitigated", "challenge")
            exchange.sendResponseHeaders(if (cleared) 200 else 403, body.size.toLong())
            exchange.responseBody.use { it.write(body) }
        }
        server.start()
    }

    @After
    fun stop() {
        server.stop(0)
        HostNetwork.cookieJar = savedJar
        HostNetwork.browserCheckSolver = savedSolver
    }

    private val client
        get() =
            OkHttpClient
                .Builder()
                .cookieJar(HostNetwork.cookieJar)
                .addInterceptor(CloudflareInterceptor())
                .build()

    private fun get(userAgent: String = "Extension UA") =
        client.newCall(Request.Builder().url("$base/manga?page=1").header("User-Agent", userAgent).build()).execute().use { it.body.string() }

    private fun passing(seenAgent: MutableList<String> = mutableListOf()) =
        BrowserCheckSolver { url, userAgent, _ ->
            solves.incrementAndGet()
            seenAgent += userAgent
            Thread.sleep(200)
            HostNetwork.cookieJar.saveFromResponse(url, listOf(Cookie.parse(url, "cf_clearance=good; Path=/")!!))
            true
        }

    @Test
    fun solvesAndSendsAgain() {
        val agents = mutableListOf<String>()
        HostNetwork.browserCheckSolver = passing(agents)
        assertEquals("the page", get())
        assertEquals(listOf("Extension UA"), agents)
    }

    @Test
    fun parallelRequestsWaitForOneSolve() {
        HostNetwork.browserCheckSolver = passing()
        val pool = Executors.newFixedThreadPool(6)
        val results = (1..6).map { pool.submit<String> { get() } }.map { it.get(20, TimeUnit.SECONDS) }
        pool.shutdown()
        assertTrue(results.all { it == "the page" })
        assertEquals(1, solves.get())
    }

    @Test
    fun uncheckableGoesToTheReader() {
        HostNetwork.browserCheckSolver = BrowserCheckSolver { _, _, _ -> false }
        try {
            get("Custom UA")
            fail("expected a browser check")
        } catch (e: BrowserCheckException) {
            assertEquals("$base/", e.url)
            assertEquals("Custom UA", e.userAgent)
        }
    }
}
