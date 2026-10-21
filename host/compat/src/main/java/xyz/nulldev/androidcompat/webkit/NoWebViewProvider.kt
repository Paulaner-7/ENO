/*
 * Copyright (C) Contributors to the Suwayomi project
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// SCANCORE: the extension host has no embedded browser. Extensions that
// open a WebView get a clear error; browser checks are passed in SCANCORE's
// own window and reach the host as cookies.
package xyz.nulldev.androidcompat.webkit

import android.webkit.WebView
import android.webkit.WebViewProvider
import java.lang.reflect.Proxy

object NoWebViewProvider {
    fun create(view: WebView): WebViewProvider =
        Proxy.newProxyInstance(WebViewProvider::class.java.classLoader, arrayOf(WebViewProvider::class.java)) { _, method, _ ->
            when (method.name) {
                "toString" -> "NoWebViewProvider"
                "hashCode" -> view.hashCode()
                "equals" -> false
                else -> throw UnsupportedOperationException(
                    "This extension needs a web view, which SCANCORE's extension host does not have.",
                )
            }
        } as WebViewProvider
}
