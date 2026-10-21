// A tiny Aniyomi extension for SCANCORE's tests, written against the same
// API real anime extensions use: hosters (lib 16), one of them lazy and one
// with its videos already listed, a video that needs resolving, an anime
// with seasons, filters, and a setting. It reads the fixture site on
// fixture.test that the tests serve (tests/fixtures/host/anime).
package com.mediumdev.scancore.testanime

import android.app.Application
import android.content.SharedPreferences
import androidx.preference.SwitchPreferenceCompat
import eu.kanade.tachiyomi.animesource.ConfigurableAnimeSource
import eu.kanade.tachiyomi.animesource.model.AnimeFilter
import eu.kanade.tachiyomi.animesource.model.AnimeFilterList
import eu.kanade.tachiyomi.animesource.model.AnimesPage
import eu.kanade.tachiyomi.animesource.model.FetchType
import eu.kanade.tachiyomi.animesource.model.Hoster
import eu.kanade.tachiyomi.animesource.model.SAnime
import eu.kanade.tachiyomi.animesource.model.SEpisode
import eu.kanade.tachiyomi.animesource.model.Track
import eu.kanade.tachiyomi.animesource.model.Video
import eu.kanade.tachiyomi.animesource.online.AnimeHttpSource
import eu.kanade.tachiyomi.network.GET
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.float
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import okhttp3.Headers
import okhttp3.HttpUrl.Companion.toHttpUrl
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import uy.kohesive.injekt.Injekt
import uy.kohesive.injekt.api.get
import uy.kohesive.injekt.injectLazy
import java.text.SimpleDateFormat
import java.util.Locale

// Shared by both test sources.
abstract class FixtureAnimeSource : AnimeHttpSource() {
    override val lang = "en"
    override val supportsLatest = true
    override val baseUrl: String = System.getProperty("scancore.testext.base") ?: "http://fixture.test"

    protected val json: Json by injectLazy()
    private val dates = SimpleDateFormat("yyyy-MM-dd", Locale.US)

    // Every request of these sources carries a header the fixture site
    // checks, so the tests see the source's own client was used.
    override val client: OkHttpClient =
        network.client
            .newBuilder()
            .addInterceptor { chain -> chain.proceed(chain.request().newBuilder().header("X-Test-Source", "1").build()) }
            .build()

    override fun headersBuilder(): Headers.Builder = super.headersBuilder().add("Referer", "$baseUrl/")

    protected fun Response.obj(): JsonObject = json.parseToJsonElement(body.string()).jsonObject

    protected fun animes(response: Response): AnimesPage {
        val root = response.obj()
        val items =
            root["items"]!!.jsonArray.map {
                val o = it.jsonObject
                SAnime.create().apply {
                    url = o["url"]!!.jsonPrimitive.content
                    title = o["title"]!!.jsonPrimitive.content
                    thumbnail_url = o["cover"]?.jsonPrimitive?.content
                    if (o["seasons"]?.jsonPrimitive?.boolean == true) fetch_type = FetchType.Seasons
                }
            }
        return AnimesPage(items, root["next"]?.jsonPrimitive?.content == "true")
    }

    override fun popularAnimeRequest(page: Int) = GET("$baseUrl/anime/popular?page=$page", headers)

    override fun popularAnimeParse(response: Response) = animes(response)

    override fun latestUpdatesRequest(page: Int) = GET("$baseUrl/anime/popular?page=$page&latest=1", headers)

    override fun latestUpdatesParse(response: Response) = animes(response)

    override fun searchAnimeRequest(
        page: Int,
        query: String,
        filters: AnimeFilterList,
    ): Request {
        val url = "$baseUrl/anime/search".toHttpUrl().newBuilder().addQueryParameter("q", query)
        filters.forEach { if (it is GenreFilter && it.state > 0) url.addQueryParameter("genre", it.values[it.state]) }
        return GET(url.build(), headers)
    }

    override fun searchAnimeParse(response: Response) = animes(response)

    override fun animeDetailsParse(response: Response): SAnime {
        val o = response.obj()
        return SAnime.create().apply {
            title = o["title"]!!.jsonPrimitive.content
            author = o["studio"]?.jsonPrimitive?.content
            description = o["description"]?.jsonPrimitive?.content
            genre = o["genres"]?.jsonArray?.joinToString(", ") { it.jsonPrimitive.content }
            background_url = o["background"]?.jsonPrimitive?.content?.let { baseUrl + it }
            status = SAnime.ONGOING
        }
    }

    override fun episodeListRequest(anime: SAnime) = GET("$baseUrl${anime.url}/episodes", headers)

    override fun episodeListParse(response: Response): List<SEpisode> =
        response.obj()["episodes"]!!.jsonArray.map {
            val e = it.jsonObject
            SEpisode.create().apply {
                url = e["url"]!!.jsonPrimitive.content
                name = e["name"]!!.jsonPrimitive.content
                episode_number = e["number"]!!.jsonPrimitive.float
                date_upload = dates.parse(e["date"]!!.jsonPrimitive.content)!!.time
                fillermark = e["filler"]?.jsonPrimitive?.boolean == true
            }
        }

    override fun getFilterList() = AnimeFilterList(AnimeFilter.Header("Filters"), GenreFilter())

    class GenreFilter : AnimeFilter.Select<String>("Genre", arrayOf("All", "Action", "Comedy"))
}

class TestAnimeSource :
    FixtureAnimeSource(),
    ConfigurableAnimeSource {
    override val name = "Test Anime"

    private val preferences: SharedPreferences by lazy {
        Injekt.get<Application>().getSharedPreferences("source_$id", 0x0000)
    }

    override fun seasonListRequest(anime: SAnime) = GET("$baseUrl${anime.url}/seasons", headers)

    override fun seasonListParse(response: Response): List<SAnime> =
        response.obj()["seasons"]!!.jsonArray.map {
            val o = it.jsonObject
            SAnime.create().apply {
                url = o["url"]!!.jsonPrimitive.content
                title = o["title"]!!.jsonPrimitive.content
                season_number = o["number"]!!.jsonPrimitive.int.toDouble()
            }
        }

    override fun hosterListRequest(episode: SEpisode) = GET("$baseUrl${episode.url}/hosters?dub=${preferences.getBoolean("dub", false)}", headers)

    override fun hosterListParse(response: Response): List<Hoster> =
        response.obj()["hosters"]!!.jsonArray.map {
            val o = it.jsonObject
            Hoster(
                hosterUrl = o["url"]?.jsonPrimitive?.content?.let { path -> baseUrl + path } ?: "",
                hosterName = o["name"]!!.jsonPrimitive.content,
                videoList = o["videos"]?.jsonArray?.map { v -> video(v.jsonObject) },
                lazy = o["lazy"]?.jsonPrimitive?.boolean == true,
            )
        }

    override fun videoListParse(
        response: Response,
        hoster: Hoster,
    ): List<Video> = response.obj()["videos"]!!.jsonArray.map { video(it.jsonObject) }

    private fun video(o: JsonObject): Video {
        val path = o["url"]!!.jsonPrimitive.content
        return Video(
            videoUrl = if (path.isEmpty()) "" else baseUrl + path,
            videoTitle = o["title"]!!.jsonPrimitive.content,
            resolution = o["resolution"]?.jsonPrimitive?.int,
            preferred = o["preferred"]?.jsonPrimitive?.boolean == true,
            subtitleTracks = o["subtitles"]?.jsonArray?.map { t -> Track(baseUrl + t.jsonObject["url"]!!.jsonPrimitive.content, t.jsonObject["lang"]!!.jsonPrimitive.content) }.orEmpty(),
            initialized = path.isNotEmpty(),
        )
    }

    // The address of a video listed without one comes from another page.
    override suspend fun resolveVideo(video: Video): Video? {
        if (video.videoUrl.isNotEmpty()) return video
        val target = client.newCall(GET("$baseUrl/resolve", headers)).execute().use { it.obj()["url"]!!.jsonPrimitive.content }
        return video.copy(videoUrl = baseUrl + target)
    }

    // Higher resolutions first.
    override fun List<Video>.sortVideos(): List<Video> = sortedByDescending { it.resolution ?: 0 }

    override fun setupPreferenceScreen(screen: androidx.preference.PreferenceScreen) {
        screen.addPreference(
            SwitchPreferenceCompat(screen.context).apply {
                key = "dub"
                title = "Prefer dubbed"
                setDefaultValue(false)
            },
        )
    }
}

// Written as extensions were before hosters (lib 14): videos straight from
// the episode, each with the page its address is on.
class TestClassicAnimeSource : FixtureAnimeSource() {
    override val name = "Test Anime Classic"

    @Suppress("DEPRECATION")
    override fun videoListRequest(episode: SEpisode) = GET("$baseUrl${episode.url}/videos", headers)

    @Suppress("DEPRECATION")
    override fun videoListParse(response: Response): List<Video> =
        response.obj()["videos"]!!.jsonArray.map {
            val o = it.jsonObject
            // The lib 14 constructor (subtitleTracks names that one).
            Video(url = baseUrl + o["page"]!!.jsonPrimitive.content, quality = o["title"]!!.jsonPrimitive.content, videoUrl = null, subtitleTracks = emptyList())
        }

    @Suppress("DEPRECATION")
    override fun videoUrlParse(response: Response) = baseUrl + response.obj()["url"]!!.jsonPrimitive.content
}
