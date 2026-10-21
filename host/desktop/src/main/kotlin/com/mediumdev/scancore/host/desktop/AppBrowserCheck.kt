package com.mediumdev.scancore.host.desktop

import com.mediumdev.scancore.host.RpcServer
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

// On desktop the host has no browser of its own: SCANCORE passes the check
// in a hidden page of its embedded browser (src/app/BrowserCheck.cpp) with
// the extension's user agent and headers, and answers with the site's
// cookies once a new clearance arrived, or none.
class AppBrowserCheck(
    private val server: RpcServer,
) : BrowserCheckSolver {
    override fun solve(
        url: HttpUrl,
        userAgent: String,
        headers: Headers,
    ): Boolean {
        val params =
            buildJsonObject {
                put("url", url.toString())
                put("userAgent", userAgent)
                put(
                    "headers",
                    buildJsonObject {
                        for ((name, value) in headers) if (name.lowercase() !in skipped) put(name, value)
                    },
                )
            }
        // SCANCORE gives up after 30 seconds; a little longer here so its answer is not missed.
        val answer = server.ask("solveBrowserCheck", params, 40_000) as? JsonObject ?: return false
        val cookies = (answer["cookies"] as? JsonArray).orEmpty().mapNotNull { Cookie.parse(url, it.jsonPrimitive.content) }
        if (cookies.none { it.name == "cf_clearance" }) return false
        HostNetwork.cookieJar.saveFromResponse(url, cookies)
        return true
    }

    // The browser sends its own; a Cookie header would hold the old clearance.
    private val skipped = setOf("cookie", "host", "content-length", "accept-encoding", "connection", "user-agent")
}
