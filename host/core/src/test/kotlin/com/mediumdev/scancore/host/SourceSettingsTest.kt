package com.mediumdev.scancore.host

import eu.kanade.tachiyomi.animesource.AnimeSource
import eu.kanade.tachiyomi.animesource.ConfigurableAnimeSource
import eu.kanade.tachiyomi.source.ConfigurableSource
import eu.kanade.tachiyomi.source.Source
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import org.junit.Assert.assertEquals
import org.junit.Test
import java.lang.reflect.Proxy

class SourceSettingsTest {
    // Real host dispatch with sources that do not implement the optional
    // settings interface. No APK, Android SDK or network is needed.
    private fun host(anime: Boolean, configurable: Boolean = false, described: JsonArray = JsonArray(emptyList())): ExtensionHost {
        val api = if (anime) AnimeSource::class.java else Source::class.java
        val interfaces = if (configurable) arrayOf(api, if (anime) ConfigurableAnimeSource::class.java else ConfigurableSource::class.java) else arrayOf(api)
        val source = Proxy.newProxyInstance(api.classLoader, interfaces) { _, method, _ ->
            when (method.name) {
                "getId" -> 1L
                "getName" -> "Source without settings"
                "getLang" -> "en"
                "getSupportsLatest" -> false
                else -> throw AssertionError("Unexpected source call: ${method.name}")
            }
        }
        val loader = object : ExtensionLoader {
            override val libs = emptyList<String>()
            override val animeLibs = emptyList<String>()
            override fun load(pkg: String, path: String, signer: String?) =
                if (anime) LoadedExtension(anime = listOf(source as AnimeSource)) else LoadedExtension(sources = listOf(source as Source))
        }
        val bridge = object : SettingsBridge {
            override fun describe(source: Configurable): JsonArray {
                check(configurable) { "No settings bridge expected" }
                return described
            }
            override fun set(source: Configurable, key: String, value: JsonElement) = throw AssertionError("No settings bridge expected")
        }
        return ExtensionHost(loader, MemoStore(null), "test", bridge)
    }

    private val params = buildJsonObject {
        put("pkg", "test.no.settings")
        put("apk", "unused")
        put("source", "1")
    }

    @Test
    fun comicWithoutSettingsReturnsEmptyPreferences() = runBlocking {
        assertEquals(JsonArray(emptyList()), host(false).handle("preferences", params))
    }

    @Test
    fun animeWithoutSettingsReturnsEmptyPreferences() = runBlocking {
        assertEquals(JsonArray(emptyList()), host(true).handle("preferences", params))
    }

    @Test
    fun configurableSourcesPreserveSettings() = runBlocking {
        val settings = JsonArray(listOf(buildJsonObject { put("key", "quality") }))
        for (anime in listOf(false, true)) {
            assertEquals(settings, host(anime, true, settings).handle("preferences", params))
        }
    }

    @Test
    fun writingUnsupportedSettingsStillFails() = runBlocking {
        for (anime in listOf(false, true)) {
            try {
                host(anime).handle("setPreference", params)
                throw AssertionError("Unsupported settings write succeeded")
            } catch (e: HostError) {
                assertEquals(501, e.status)
            }
        }
    }
}
