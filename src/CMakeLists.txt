# Core: networking, HTML, storage and every source adapter. Kept free of QML
# so tests link it directly.
add_library(scancore_core STATIC
    core/Cookies.cpp
    core/Database.cpp
    core/Html.cpp
    core/Http.cpp
    core/ImageCache.cpp
    core/Parallel.cpp
    core/Text.cpp
    sources/Manifest.cpp
    sources/comics/ApiEngines.cpp
    sources/comics/ComicSource.cpp
    sources/comics/PageEngines.cpp
    sources/comics/Registry.cpp
    sources/local/Local.cpp
    sources/local/LocalComics.cpp
    sources/local/LocalNovels.cpp
    sources/novels/Engines.cpp
    sources/novels/LnCrawl.cpp
    sources/novels/Registry.cpp
    sources/novels/TextBlocks.cpp
)
target_include_directories(scancore_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Manifest entries can ask for a minimum app version.
target_compile_definitions(scancore_core PRIVATE SCANCORE_VERSION="${PROJECT_VERSION}")
target_link_libraries(scancore_core
    PUBLIC Qt6::Core Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Qml
    # QZipReader, for CBZ and EPUB files.
    PRIVATE scancore_lexbor Qt6::CorePrivate)

set(SCANCORE_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND SCANCORE_PLATFORM_SOURCES ${PROJECT_SOURCE_DIR}/packaging/windows/scancore.rc)
elseif(APPLE)
    # ENO.icns + Assets.car are compiled from packaging/macos/ENO.icon with Xcode's actool
    # (Liquid Glass app icon); the compiled outputs are committed so builds don't need Xcode.
    set(SCANCORE_ICNS ${PROJECT_SOURCE_DIR}/packaging/macos/ENO.icns ${PROJECT_SOURCE_DIR}/packaging/macos/Assets.car)
    set_source_files_properties(${SCANCORE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND SCANCORE_PLATFORM_SOURCES ${SCANCORE_ICNS})
endif()

qt_add_executable(scancore WIN32 MACOSX_BUNDLE main.cpp ${SCANCORE_PLATFORM_SOURCES})
set_target_properties(scancore PROPERTIES
    OUTPUT_NAME ENO
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    MACOSX_BUNDLE_GUI_IDENTIFIER com.vittorio.eno
    MACOSX_BUNDLE_BUNDLE_NAME ENO
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_ICON_FILE ENO
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in)
target_compile_definitions(scancore PRIVATE SCANCORE_VERSION="${PROJECT_VERSION}"
    SCANCORE_DISCORD_APP_ID="${SCANCORE_DISCORD_APP_ID}"
    SCANCORE_ANILIST_CLIENT_ID="${SCANCORE_ANILIST_CLIENT_ID}"
    SCANCORE_MAL_CLIENT_ID="${SCANCORE_MAL_CLIENT_ID}"
    SCANCORE_COMICK_CLIENT_ID="${SCANCORE_COMICK_CLIENT_ID}")
if(NOT SCANCORE_WITH_NOVEL_PACK_INSTALLER)
    target_compile_definitions(scancore PRIVATE SCANCORE_DISABLE_NOVEL_PACK)
    target_compile_definitions(scancore_core PRIVATE SCANCORE_DISABLE_NOVEL_PACK)
endif()

file(GLOB_RECURSE SCANCORE_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml)
if(ANDROID)
    list(REMOVE_ITEM SCANCORE_QML qml/components/WebPlayer.qml qml/components/WebCheckView.qml)
endif()
if(NOT SCANCORE_WITH_SYSTEM_WEBVIEW)
    list(REMOVE_ITEM SCANCORE_QML qml/components/WebPlayerSystem.qml)
endif()
set_source_files_properties(qml/theme/Theme.qml qml/theme/Motion.qml qml/Router.qml qml/Keymap.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(scancore
    URI ScanCore
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${SCANCORE_QML}
    SOURCES
        app/Account.cpp app/Account.h
        app/Activity.cpp app/Activity.h
        app/Anime.cpp app/Anime.h
        app/App.cpp app/App.h
        app/Comics.cpp app/Comics.h
        app/Customize.cpp app/Customize.h
        app/Downloads.cpp app/Downloads.h
        app/Extensions.cpp app/Extensions.h
        app/Images.cpp app/Images.h
        app/Importer.cpp app/Importer.h
        app/SingleInstance.cpp app/SingleInstance.h
        app/Trackers.cpp app/Trackers.h
        app/Sync.cpp app/Sync.h
        app/Library.cpp app/Library.h
        app/Novels.cpp app/Novels.h
        app/Presence.cpp app/Presence.h
        app/Updater.cpp app/Updater.h
        app/WebBridge.cpp app/WebBridge.h
        core/Task.cpp core/Task.h
)

# The interface in other languages. `cmake --build <dir> --target
# update_translations` refreshes the .ts files from the code.
if(TARGET Qt6::lupdate)
    qt_add_translations(scancore
        TS_FILES
            ${PROJECT_SOURCE_DIR}/i18n/scancore_es.ts
            ${PROJECT_SOURCE_DIR}/i18n/scancore_fr.ts
            ${PROJECT_SOURCE_DIR}/i18n/scancore_id.ts
            ${PROJECT_SOURCE_DIR}/i18n/scancore_pt_BR.ts
        RESOURCE_PREFIX /i18n
        LUPDATE_OPTIONS -no-obsolete -locations none)
else()
    message(STATUS "Qt Linguist tools not found; building without translations")
endif()

file(GLOB SCANCORE_ICONS RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/resources/icons/*.svg)
qt_add_resources(scancore assets
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES
        ${PROJECT_SOURCE_DIR}/resources/fonts/DMSans.ttf
        ${PROJECT_SOURCE_DIR}/resources/fonts/DMSans-Italic.ttf
        ${PROJECT_SOURCE_DIR}/resources/fonts/SpaceGrotesk.ttf
        ${PROJECT_SOURCE_DIR}/resources/fonts/Literata.ttf
        ${PROJECT_SOURCE_DIR}/resources/fonts/Literata-Italic.ttf
        ${PROJECT_SOURCE_DIR}/resources/branding/scancore.svg
        ${PROJECT_SOURCE_DIR}/resources/branding/shadow.png
        ${PROJECT_SOURCE_DIR}/resources/branding/eno-logo-dark.png
        ${PROJECT_SOURCE_DIR}/resources/branding/eno-logo-light.png
        ${PROJECT_SOURCE_DIR}/resources/branding/eno-symbol-dark.png
        ${PROJECT_SOURCE_DIR}/resources/branding/eno-symbol-light.png
        ${PROJECT_SOURCE_DIR}/resources/branding/eno-icon.png
        ${PROJECT_SOURCE_DIR}/resources/branding/eno-icon-glass.png)
foreach(icon ${SCANCORE_ICONS})
    list(APPEND SCANCORE_ICON_PATHS ${PROJECT_SOURCE_DIR}/${icon})
endforeach()
qt_add_resources(scancore icons
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES ${SCANCORE_ICON_PATHS})
qt_add_resources(scancore lncrawl
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES
        ${PROJECT_SOURCE_DIR}/resources/lncrawl/sources.json
        ${PROJECT_SOURCE_DIR}/resources/lncrawl/bridge.py)
qt_add_resources(scancore_core manifest
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}
    FILES ${PROJECT_SOURCE_DIR}/sources/manifest.json)

target_include_directories(scancore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app ${CMAKE_CURRENT_SOURCE_DIR}/core)

target_link_libraries(scancore PRIVATE
    scancore_core Qt6::CorePrivate Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Svg)
if(ANDROID)
    # Qt's network stack on Android needs OpenSSL shipped inside the APK.
    include(FetchContent)
    FetchContent_Declare(android_openssl
        GIT_REPOSITORY https://github.com/KDAB/android_openssl.git
        GIT_TAG b71f1470962019bd89534a2919f5925f93bc5779)
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
    add_android_openssl_libraries(scancore)
    set_target_properties(scancore PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/packaging/android
        QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
        QT_ANDROID_VERSION_CODE ${SCANCORE_ANDROID_VERSION_CODE}
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36)
else()
    target_link_libraries(scancore PRIVATE Qt6::Widgets)
endif()

if(SCANCORE_WITH_SYSTEM_WEBVIEW)
    target_link_libraries(scancore PRIVATE Qt6::WebView)
    target_compile_definitions(scancore PRIVATE SCANCORE_HAVE_WEBVIEW)
endif()
if(SCANCORE_WITH_WEBENGINE)
    target_link_libraries(scancore PRIVATE Qt6::WebEngineQuick Qt6::WebEngineCore)
    target_compile_definitions(scancore PRIVATE SCANCORE_HAVE_WEBENGINE)
endif()
if(TARGET Qt6::Pdf)
    target_link_libraries(scancore PRIVATE Qt6::Pdf)
    target_compile_definitions(scancore PRIVATE SCANCORE_HAVE_PDF)
endif()
if(SCANCORE_HAVE_MPV)
    target_link_libraries(scancore PRIVATE scancore_mpv)
    target_compile_definitions(scancore PRIVATE SCANCORE_HAVE_MPV)
endif()

if(NOT ANDROID)
    install(TARGETS scancore
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
