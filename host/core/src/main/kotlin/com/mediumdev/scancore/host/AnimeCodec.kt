package com.mediumdev.scancore.host

import eu.kanade.tachiyomi.animesource.model.AnimeFilter
import eu.kanade.tachiyomi.animesource.model.AnimeFilterList
import eu.kanade.tachiyomi.animesource.model.AnimesPage
import eu.kanade.tachiyomi.animesource.model.FetchType
import eu.kanade.tachiyomi.animesource.model.SAnime
import eu.kanade.tachiyomi.animesource.model.SEpisode
import eu.kanade.tachiyomi.animesource.model.Video
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import okhttp3.Headers
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull

// Aniyomi's models as the JSON SCANCORE reads (see src/extensions/AnimeSource.h).
object AnimeCodec {
    private fun link(
        url: String?,
        base: String?,
    ): String? {
        if (url.isNullOrEmpty() || base.isNullOrEmpty() || url.contains("://")) return url
        return base.toHttpUrlOrNull()?.resolve(url)?.toString() ?: url
    }

    fun fetchType(anime: SAnime) = if (anime.fetch_type == FetchType.Seasons) "seasons" else "episodes"

    // A listing item, or a season of an anime.
    private fun item(
        anime: SAnime,
        base: String?,
    ) = buildJsonObject {
        put("url", anime.url)
        put("title", anime.title)
        put("cover", link(anime.thumbnail_url, base))
        put("fetchType", fetchType(anime))
        if (anime.season_number >= 0) put("season", anime.season_number)
    }

    fun animes(
        page: AnimesPage,
        base: String? = null,
    ): JsonObject =
        buildJsonObject {
            put("items", buildJsonArray { page.animes.forEach { add(item(it, base)) } })
            put("hasNextPage", page.hasNextPage)
        }

    fun seasons(
        list: List<SAnime>,
        base: String? = null,
    ): JsonArray = buildJsonArray { list.forEach { add(item(it, base)) } }

    fun details(
        anime: SAnime,
        base: String? = null,
    ): JsonObject =
        buildJsonObject {
            put("title", anime.title)
            put("cover", link(anime.thumbnail_url, base))
            put("background", link(anime.background_url, base))
            put("description", anime.description)
            put("author", anime.author)
            put("artist", anime.artist)
            put("status", anime.status)
            put("genres", buildJsonArray { anime.getGenres()?.forEach { add(JsonPrimitive(it)) } })
            put("fetchType", fetchType(anime))
        }

    fun episodes(list: List<SEpisode>): JsonArray =
        buildJsonArray {
            for (episode in list) {
                add(
                    buildJsonObject {
                        put("url", episode.url)
                        put("name", episode.name)
                        put("number", episode.episode_number)
                        put("uploaded", episode.date_upload)
                        put("scanlator", episode.scanlator)
                        put("filler", episode.fillermark)
                        put("summary", episode.summary)
                        put("preview", episode.preview_url)
                    },
                )
            }
        }

    // A video as mpv needs it: its address, the headers to send (the
    // video's own, else its source's) and its tracks. `id` names it in
    // later calls (resolveVideo).
    fun video(
        id: String,
        video: Video,
        fallbackHeaders: Headers?,
    ): JsonObject =
        buildJsonObject {
            put("id", id)
            put("url", video.videoUrl)
            put("title", video.videoTitle)
            video.resolution?.let { put("resolution", it) }
            video.bitrate?.let { put("bitrate", it) }
            put("preferred", video.preferred)
            put("initialized", video.initialized)
            put("localServer", video.usesHttpServer())
            put(
                "headers",
                buildJsonObject { (video.headers ?: fallbackHeaders)?.forEach { (name, value) -> put(name, value) } },
            )
            put("subtitles", tracks(video.subtitleTracks.map { it.url to it.lang }))
            put("audio", tracks(video.audioTracks.map { it.url to it.lang }))
            put(
                "timestamps",
                buildJsonArray {
                    for (t in video.timestamps) {
                        add(
                            buildJsonObject {
                                put("start", t.start)
                                put("end", t.end)
                                put("name", t.name)
                                put("type", t.type.name)
                            },
                        )
                    }
                },
            )
            put("mpvArgs", buildJsonObject { video.mpvArgs.forEach { (k, v) -> put(k, v) } })
        }

    private fun tracks(list: List<Pair<String, String>>) =
        buildJsonArray {
            for ((url, lang) in list) {
                add(
                    buildJsonObject {
                        put("url", url)
                        put("lang", lang)
                    },
                )
            }
        }

    // ---------------------------------------------------------------- filters
    // The same JSON as Mihon's filters (Codec), from Aniyomi's classes.

    fun filters(list: AnimeFilterList): JsonArray = buildJsonArray { list.forEach { add(filter(it)) } }

    private fun filter(filter: AnimeFilter<*>): JsonObject =
        buildJsonObject {
            put("name", filter.name)
            when (filter) {
                is AnimeFilter.Header -> put("type", "header")
                is AnimeFilter.Separator -> put("type", "separator")
                is AnimeFilter.Select<*> -> {
                    put("type", "select")
                    put("values", buildJsonArray { filter.values.forEach { add(JsonPrimitive(it.toString())) } })
                    put("state", filter.state)
                }
                is AnimeFilter.Text -> {
                    put("type", "text")
                    put("state", filter.state)
                }
                is AnimeFilter.CheckBox -> {
                    put("type", "checkbox")
                    put("state", filter.state)
                }
                is AnimeFilter.TriState -> {
                    put("type", "tristate")
                    put("state", filter.state)
                }
                is AnimeFilter.Sort -> {
                    put("type", "sort")
                    put("values", buildJsonArray { filter.values.forEach { add(JsonPrimitive(it)) } })
                    val selection = filter.state
                    put(
                        "state",
                        if (selection == null) {
                            JsonNull
                        } else {
                            buildJsonObject {
                                put("index", selection.index)
                                put("ascending", selection.ascending)
                            }
                        },
                    )
                }
                is AnimeFilter.Group<*> -> {
                    put("type", "group")
                    put(
                        "filters",
                        buildJsonArray {
                            filter.state.forEach { child -> if (child is AnimeFilter<*>) add(filter(child)) else add(JsonNull) }
                        },
                    )
                }
            }
        }

    fun applyFilters(
        list: AnimeFilterList,
        states: JsonArray,
    ): AnimeFilterList {
        list.forEachIndexed { i, filter -> states.getOrNull(i)?.let { apply(filter, it) } }
        return list
    }

    @Suppress("UNCHECKED_CAST")
    private fun apply(
        filter: AnimeFilter<*>,
        json: JsonElement,
    ) {
        val o = json as? JsonObject ?: return
        val state = o["state"]
        when (filter) {
            is AnimeFilter.Select<*> -> state?.jsonPrimitive?.intOrNull?.let { filter.state = it.coerceIn(0, (filter.values.size - 1).coerceAtLeast(0)) }
            is AnimeFilter.Text -> state?.jsonPrimitive?.contentOrNull?.let { filter.state = it }
            is AnimeFilter.CheckBox -> state?.jsonPrimitive?.booleanOrNull?.let { filter.state = it }
            is AnimeFilter.TriState -> state?.jsonPrimitive?.intOrNull?.let { filter.state = it.coerceIn(0, 2) }
            is AnimeFilter.Sort ->
                filter.state =
                    (state as? JsonObject)?.let {
                        AnimeFilter.Sort.Selection(
                            it["index"]?.jsonPrimitive?.intOrNull ?: 0,
                            it["ascending"]?.jsonPrimitive?.booleanOrNull ?: true,
                        )
                    }
            is AnimeFilter.Group<*> -> {
                val children = o["filters"] as? JsonArray ?: return
                (filter.state as List<Any?>).forEachIndexed { i, child ->
                    if (child is AnimeFilter<*>) children.getOrNull(i)?.let { apply(child, it) }
                }
            }
            else -> {}
        }
    }
}
