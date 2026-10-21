include(FetchContent)

# lexbor: HTML parser with CSS selectors, used by every scraping adapter.
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_UTILS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lexbor
    GIT_REPOSITORY https://github.com/lexbor/lexbor.git
    GIT_TAG v2.7.0
    GIT_SHALLOW TRUE
    SYSTEM
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(lexbor)
add_library(scancore_lexbor INTERFACE)
target_link_libraries(scancore_lexbor INTERFACE lexbor_static)
target_include_directories(scancore_lexbor SYSTEM INTERFACE ${lexbor_SOURCE_DIR}/source)
target_compile_definitions(scancore_lexbor INTERFACE LEXBOR_STATIC)

# libmpv: the video player for anime from extensions. Found through
# pkg-config on Linux and macOS, or MPV_DIR (include/ + lib/) on Windows and
# Android. Android builds take the one tools/libmpv/build.sh made for their
# ABI (build/libmpv/android-<abi>) unless MPV_DIR says otherwise; its FFmpeg
# libraries go into the APK with it.
set(SCANCORE_HAVE_MPV OFF)
if(SCANCORE_WITH_MPV)
    if(ANDROID AND NOT DEFINED MPV_DIR AND NOT DEFINED ENV{MPV_DIR})
        if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "arm64-v8a")
            set(MPV_DIR ${PROJECT_SOURCE_DIR}/build/libmpv/android-arm64)
        else()
            set(MPV_DIR ${PROJECT_SOURCE_DIR}/build/libmpv/android-x86_64)
        endif()
    endif()
    if(DEFINED ENV{MPV_DIR} OR DEFINED MPV_DIR)
        if(NOT DEFINED MPV_DIR)
            set(MPV_DIR $ENV{MPV_DIR})
        endif()
        find_path(MPV_INCLUDE_DIR mpv/client.h HINTS ${MPV_DIR}/include NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        find_library(MPV_LIBRARY NAMES mpv libmpv libmpv.dll.a HINTS ${MPV_DIR} ${MPV_DIR}/lib NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        if(MPV_INCLUDE_DIR AND MPV_LIBRARY)
            add_library(scancore_mpv INTERFACE)
            target_include_directories(scancore_mpv INTERFACE ${MPV_INCLUDE_DIR})
            target_link_libraries(scancore_mpv INTERFACE ${MPV_LIBRARY})
            if(ANDROID)
                # MediaCodec needs the Java VM, which the app hands to libavcodec.
                target_link_libraries(scancore_mpv INTERFACE ${MPV_DIR}/lib/libavcodec.so)
                file(GLOB SCANCORE_MPV_ANDROID_LIBS ${MPV_DIR}/lib/*.so)
            endif()
            set(SCANCORE_HAVE_MPV ON)
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(MPV QUIET IMPORTED_TARGET mpv)
            if(MPV_FOUND)
                add_library(scancore_mpv INTERFACE)
                target_link_libraries(scancore_mpv INTERFACE PkgConfig::MPV)
                set(SCANCORE_HAVE_MPV ON)
            endif()
        endif()
    endif()
    if(NOT SCANCORE_HAVE_MPV)
        if(ANDROID)
            message(WARNING "libmpv not found in ${MPV_DIR} (run tools/libmpv/build.sh); videos from anime extensions will not play")
        else()
            message(WARNING "libmpv not found; videos from anime extensions will not play")
        endif()
    endif()
endif()
