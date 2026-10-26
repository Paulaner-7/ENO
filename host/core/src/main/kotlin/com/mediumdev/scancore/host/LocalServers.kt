package com.mediumdev.scancore.host

import eu.kanade.tachiyomi.animesource.AnimeSource
import eu.kanade.tachiyomi.animesource.model.HttpServer
import eu.kanade.tachiyomi.animesource.model.Video
import eu.kanade.tachiyomi.animesource.online.AnimeHttpSource
import java.util.concurrent.ConcurrentHashMap

// Videos some Aniyomi extensions serve from this device: their address is
// the placeholder http://localhost:1 until the source's own server runs
// (Aniyomi's player does the same). One server per source, started the
// first time a video needs it and kept while the host runs; a server that
// stopped is started again.
object LocalServers {
    private val servers = ConcurrentHashMap<Long, HttpServer>()

    // `video` with the server's address, or as it was when it needs none
    // (or the source has no server to give).
    fun serve(
        source: AnimeSource,
        video: Video,
    ): Video {
        if (!video.usesHttpServer()) return video
        val http = source as? AnimeHttpSource ?: return video
        val server =
            synchronized(this) {
                servers[source.id]?.takeIf { it.isRunning() }
                    ?: http.createHttpServer()?.also {
                        it.start()
                        servers[source.id] = it
                    }
            } ?: return video
        if (!server.isRunning() || server.listeningPort <= 0) return video
        return video.copyHttpServer(server.listeningPort)
    }
}
