// Aniyomi's HttpServer (Apache-2.0, see NOTICE.md): a NanoHTTPD server some
// extensions subclass to serve a video from this device; the host starts it
// when a video needs it (AnimeCalls, LocalServers).
// SCANCORE: it listens on this device only (127.0.0.1), not every network
// the device is on, and a failure goes to the host's log (stderr).
package eu.kanade.tachiyomi.animesource.model

import fi.iki.elonen.NanoHTTPD

open class HttpServer : NanoHTTPD("127.0.0.1", 0) {
    val url: String
        get() = "http://localhost:$listeningPort"

    fun isRunning(): Boolean = isRunning

    @Volatile
    private var isRunning = false

    override fun start() {
        try {
            super.start()
            isRunning = true
        } catch (e: Exception) {
            System.err.println("A local video server could not start: $e")
        }
    }

    override fun stop() {
        super.stop()
        isRunning = false
    }

    companion object {
        const val PLACEHOLDER_URL = "http://localhost:1"
    }
}
