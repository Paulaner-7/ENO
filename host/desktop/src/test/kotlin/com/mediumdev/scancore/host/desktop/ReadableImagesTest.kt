package com.mediumdev.scancore.host.desktop

import com.sun.net.httpserver.HttpServer
import eu.kanade.tachiyomi.network.ReadableImagesInterceptor
import okhttp3.OkHttpClient
import okhttp3.Request
import org.junit.Assert.assertEquals
import org.junit.Test
import java.net.InetSocketAddress

// Image formats Qt cannot show are taken out of the Accept header an
// extension sends; everything else goes through unchanged.
class ReadableImagesTest {
    private fun acceptSeen(accept: String?): String {
        val server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)
        server.createContext("/") { exchange ->
            val body = (exchange.requestHeaders.getFirst("Accept") ?: "(none)").toByteArray()
            exchange.sendResponseHeaders(200, body.size.toLong())
            exchange.responseBody.use { it.write(body) }
        }
        server.start()
        try {
            val client = OkHttpClient.Builder().addNetworkInterceptor(ReadableImagesInterceptor()).build()
            val request = Request.Builder().url("http://127.0.0.1:${server.address.port}/img.jpg").apply { accept?.let { header("Accept", it) } }.build()
            return client.newCall(request).execute().use { it.body.string() }
        } finally {
            server.stop(0)
        }
    }

    @Test
    fun dropsFormatsQtCannotRead() {
        assertEquals("image/webp,image/apng,image/*,*/*;q=0.8", acceptSeen("image/avif,image/webp,image/apng,image/*,*/*;q=0.8"))
        assertEquals("image/webp", acceptSeen("image/jxl, image/webp, image/heic;q=0.9").replace(" ", ""))
        assertEquals("*/*", acceptSeen("image/avif"))
    }

    @Test
    fun leavesOtherHeadersAlone() {
        assertEquals("application/json", acceptSeen("application/json"))
        assertEquals("(none)", acceptSeen(null))
    }
}
