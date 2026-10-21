plugins {
    alias(libs.plugins.kotlin.jvm) apply false
    alias(libs.plugins.kotlin.serialization) apply false
    alias(libs.plugins.android.library) apply false
}

subprojects {
    // Extensions are Java 8 bytecode; the host runs on Java 17+.
    val jdkVersion = if (JavaVersion.current().isCompatibleWith(JavaVersion.VERSION_21)) 21 else 17
    plugins.withId("org.jetbrains.kotlin.jvm") {
        extensions.configure<org.jetbrains.kotlin.gradle.dsl.KotlinJvmProjectExtension> {
            jvmToolchain(jdkVersion)
        }
    }
    plugins.withType<JavaPlugin> {
        extensions.configure<JavaPluginExtension> {
            toolchain.languageVersion.set(JavaLanguageVersion.of(jdkVersion))
        }
    }
}
