# The Kotlin extension host (host/): runs Mihon and Aniyomi extensions.
# Built with its Gradle wrapper, which downloads Gradle and the host's
# libraries the first time. Needs a JDK 17 or newer. On desktop it is a
# development host (users download the Extensions runtime; without a JDK
# the app builds without one); on Android it is an AAR inside the APK.
set(SCANCORE_HAVE_EXTENSION_HOST OFF)
set(SCANCORE_HAVE_ANDROID_HOST OFF)
if(SCANCORE_WITH_EXTENSION_HOST AND ANDROID)
    find_package(Java 17 QUIET COMPONENTS Development)
    if(NOT Java_FOUND)
        message(FATAL_ERROR "The Android extension host needs a JDK 17+ (or -DSCANCORE_WITH_EXTENSION_HOST=OFF)")
    endif()
    set(SCANCORE_HOST_DIR ${PROJECT_SOURCE_DIR}/host)
    if(CMAKE_HOST_WIN32)
        # No daemon: see below.
        set(SCANCORE_GRADLE ${SCANCORE_HOST_DIR}/gradlew.bat --no-daemon)
    else()
        set(SCANCORE_GRADLE ${SCANCORE_HOST_DIR}/gradlew)
    endif()
    # The AAR and its Maven libraries (scancore-host.deps), for
    # packaging/android/build.gradle.
    set(SCANCORE_HOST_APP_DIR ${SCANCORE_HOST_DIR}/android/build/app)
    file(GLOB_RECURSE SCANCORE_HOST_SOURCES CONFIGURE_DEPENDS
        ${SCANCORE_HOST_DIR}/api/*.kt ${SCANCORE_HOST_DIR}/core/*.kt ${SCANCORE_HOST_DIR}/android/*.kt
        ${SCANCORE_HOST_DIR}/*.kts ${SCANCORE_HOST_DIR}/*.toml ${SCANCORE_HOST_DIR}/android/*.pro)
    list(FILTER SCANCORE_HOST_SOURCES EXCLUDE REGEX "/build/")
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/scancore_host_android.stamp
        # The SDK this build uses (host/settings.gradle.kts reads it).
        COMMAND ${CMAKE_COMMAND} -E env ANDROID_HOME=${ANDROID_SDK_ROOT}
            ${SCANCORE_GRADLE} -p ${SCANCORE_HOST_DIR} --console=plain -q :android:appBundle
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/scancore_host_android.stamp
        DEPENDS ${SCANCORE_HOST_SOURCES}
        WORKING_DIRECTORY ${SCANCORE_HOST_DIR}
        COMMENT "Building the Android extension host (Gradle)"
        USES_TERMINAL
        VERBATIM)
    add_custom_target(scancore_host ALL DEPENDS ${CMAKE_BINARY_DIR}/scancore_host_android.stamp)
    set(SCANCORE_HAVE_ANDROID_HOST ON)
endif()
if(SCANCORE_WITH_EXTENSION_HOST AND NOT ANDROID)
    find_package(Java 17 QUIET COMPONENTS Development)
    if(NOT Java_FOUND)
        message(WARNING "No JDK 17+: building without the development extension host")
    else()
        set(SCANCORE_HOST_DIR ${PROJECT_SOURCE_DIR}/host)
        if(WIN32)
            # No daemon: on Windows it inherits the build's output handles,
            # and Ninja and CTest wait for it to go idle (three hours).
            set(SCANCORE_GRADLE ${SCANCORE_HOST_DIR}/gradlew.bat --no-daemon)
        else()
            set(SCANCORE_GRADLE ${SCANCORE_HOST_DIR}/gradlew)
        endif()
        # Where `installDist` puts the host and its jars, and the test APKs.
        set(SCANCORE_HOST_LIB_DIR ${SCANCORE_HOST_DIR}/desktop/build/install/scancore-host/lib)
        set(SCANCORE_TESTEXT_APK ${SCANCORE_HOST_DIR}/testext/build/apk/testext.apk)
        set(SCANCORE_TESTANIME_APK ${SCANCORE_HOST_DIR}/testext/build/animeapk/testanime.apk)
        file(GLOB_RECURSE SCANCORE_HOST_SOURCES CONFIGURE_DEPENDS
            ${SCANCORE_HOST_DIR}/*.kt ${SCANCORE_HOST_DIR}/*.java ${SCANCORE_HOST_DIR}/*.kts
            ${SCANCORE_HOST_DIR}/*.toml ${SCANCORE_HOST_DIR}/testext/src/main/android/* ${SCANCORE_HOST_DIR}/testext/src/anime/android/*)
        list(FILTER SCANCORE_HOST_SOURCES EXCLUDE REGEX "/build/")
        # Gradle only runs when a host file changed.
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/scancore_host.stamp
            COMMAND ${SCANCORE_GRADLE} -p ${SCANCORE_HOST_DIR} --console=plain -q :desktop:installDist :testext:apk
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/scancore_host.stamp
            DEPENDS ${SCANCORE_HOST_SOURCES}
            WORKING_DIRECTORY ${SCANCORE_HOST_DIR}
            COMMENT "Building the extension host (Gradle)"
            USES_TERMINAL
            VERBATIM)
        add_custom_target(scancore_host ALL DEPENDS ${CMAKE_BINARY_DIR}/scancore_host.stamp)
        set(SCANCORE_HAVE_EXTENSION_HOST ON)
    endif()
endif()
