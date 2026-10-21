// SCANCORE: Aniyomi's HttpServer is a NanoHTTPD server some extensions start
// to serve a video from this device. The host runs no local servers, so this
// keeps only its public face: extensions that use one still load, and videos
// with the placeholder address are reported as unsupported.
package eu.kanade.tachiyomi.animesource.model

open class HttpServer {
    open val listeningPort: Int get() = 0

    val url: String
        get() = "http://localhost:$listeningPort"

    fun isRunning(): Boolean = false

    open fun start() {
        throw UnsupportedOperationException("SCANCORE runs no local video servers.")
    }

    open fun stop() {}

    companion object {
        const val PLACEHOLDER_URL = "http://localhost:1"
    }
}
