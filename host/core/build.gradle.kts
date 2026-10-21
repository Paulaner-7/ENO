// The host's protocol and calls, shared by the desktop helper and (from
// milestone 4) the Android app.
plugins {
    alias(libs.plugins.kotlin.jvm)
    `java-library`
}

dependencies {
    api(project(":api"))
    testImplementation(libs.junit)
}
