package com.mediumdev.scancore.host.desktop

import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test
import xyz.nulldev.androidcompat.webkit.CookieManagerImpl
import java.net.CookieHandler
import java.net.CookieManager
import java.net.HttpCookie
import java.net.URI

class CookieManagerTest {
    @Test
    fun preservesExistingCookieStore() {
        val previous = CookieHandler.getDefault()
        try {
            val handler = CookieManager()
            handler.cookieStore.add(URI("https://fixture.test/"), HttpCookie("existing", "kept"))
            CookieHandler.setDefault(handler)
            assertEquals("existing=kept", CookieManagerImpl().getCookie("https://fixture.test/"))
            assertSame(handler, CookieHandler.getDefault())
        } finally {
            CookieHandler.setDefault(previous)
        }
    }

    @Test
    fun acceptsOtherDefaultHandlerWithoutReplacingIt() {
        val previous = CookieHandler.getDefault()
        try {
            val handler = object : CookieHandler() {
                override fun get(uri: URI, headers: Map<String, List<String>>) = emptyMap<String, List<String>>()
                override fun put(uri: URI, headers: Map<String, List<String>>) {}
            }
            CookieHandler.setDefault(handler)
            val manager = CookieManagerImpl()
            manager.setCookie("https://fixture.test/", "setting=enabled; Path=/")
            assertEquals("setting=enabled", manager.getCookie("https://fixture.test/"))
            assertSame(handler, CookieHandler.getDefault())
        } finally {
            CookieHandler.setDefault(previous)
        }
    }

    @Test
    fun worksWithoutDefaultJavaCookieHandler() {
        val previous = CookieHandler.getDefault()
        try {
            CookieHandler.setDefault(null)
            val manager = CookieManagerImpl()
            manager.setCookie("https://fixture.test/", "setting=enabled; Path=/")
            assertEquals("setting=enabled", manager.getCookie("https://fixture.test/"))
        } finally {
            CookieHandler.setDefault(previous)
        }
    }
}
