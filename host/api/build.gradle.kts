// The extension API: Mihon's source API and network helpers (Apache-2.0,
// see NOTICE.md) with SCANCORE's NetworkHelper. Extensions are compiled
// against these classes, so names and signatures follow Mihon exactly.
plugins {
    alias(libs.plugins.kotlin.jvm)
    alias(libs.plugins.kotlin.serialization)
    `java-library`
}

dependencies {
    api(project(":compat"))
    api(libs.coroutines.core)
    api(libs.serialization.json)
    api(libs.serialization.json.okio)
    api(libs.serialization.protobuf)
    api(libs.okhttp.core)
    api(libs.okhttp.brotli)
    // Keiyoushi's extensions decompress Zstd themselves (Mihon ships it too).
    api(libs.okhttp.zstd)
    api(libs.okhttp.dnsoverhttps)
    api(libs.okio)
    api(libs.jsoup)
    api(libs.rxjava)
    api(libs.injekt)
    api(libs.nanohttpd)
}
