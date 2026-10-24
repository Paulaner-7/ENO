package com.mediumdev.scancore.host

import eu.kanade.tachiyomi.network.BrowserCheckSolver
import eu.kanade.tachiyomi.network.HostNetwork
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import okhttp3.Cookie
import okhttp3.Headers
import okhttp3.HttpUrl
import java.util.concurrent.ConcurrentHashMap

// A check the hidden browser could not pass (Cloudflare wants a tap): SCANCORE
// shows the site's check right away, over whatever is loading, and the
// request waits for it, so one tap is all it takes and nothing has to be
// tried again. Better than Mihon, which fails and leaves the reader to find
// its WebView. Only while SCANCORE is on screen; a check the reader closes
// without passing is not shown again for that site for a while (the page
// then offers its "Open browser check" button).
class ReaderBrowserCheck(
    private val server: RpcServer,
) : BrowserCheckSolver {
    private val declined = ConcurrentHashMap<String, Long>()

    override fun solve(
        url: HttpUrl,
        userAgent: String,
        headers: Headers,
    ): Boolean {
        val now = System.currentTimeMillis()
        if ((declined[url.host] ?: 0) > now) return false
        val before = clearance(url)
        val site = url.newBuilder().encodedPath("/").query(null).fragment(null).build()
        val params =
            buildJsonObject {
                put("url", site.toString())
                put("userAgent", userAgent)
            }
        // SCANCORE stops waiting after 90 seconds; a little longer here.
        val answer = server.ask("readerBrowserCheck", params, 95_000) as? JsonObject
        val cookies = (answer?.get("cookies") as? JsonArray).orEmpty().mapNotNull { Cookie.parse(url, it.jsonPrimitive.content) }
        if (cookies.isNotEmpty()) HostNetwork.cookieJar.saveFromResponse(url, cookies)
        val after = clearance(url)
        val passed = !after.isNullOrBlank() && after != before
        // Not shown at all (a search across many sources) is not declined.
        val quiet = answer?.get("quiet")?.jsonPrimitive?.content == "true"
        if (!passed && !quiet) declined[url.host] = now + 10 * 60_000
        return passed
    }

    private fun clearance(url: HttpUrl) = HostNetwork.cookieJar.loadForRequest(url).firstOrNull { it.name == "cf_clearance" }?.value
}
