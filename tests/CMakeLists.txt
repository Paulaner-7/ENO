find_package(Qt6 REQUIRED COMPONENTS Test)
# Parsers of the bundled sources, and "Read from" over them.
if(SCANCORE_WITH_BUNDLED_SOURCES)
    qt_add_executable(scancore_tests tst_parsing.cpp)
    target_link_libraries(scancore_tests PRIVATE scancore_core scancore_lexbor Qt6::Test)
    add_test(NAME parsing COMMAND scancore_tests)
    target_compile_definitions(scancore_tests PRIVATE SCANCORE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    # Tracker catalogues, content language and "Read from".
    qt_add_executable(scancore_catalogue_tests tst_catalogue.cpp)
    target_link_libraries(scancore_catalogue_tests PRIVATE scancore_core Qt6::Test)
    add_test(NAME catalogue COMMAND scancore_catalogue_tests)
    target_compile_definitions(scancore_catalogue_tests PRIVATE SCANCORE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

# The library on a scratch database: moving series between sources.
qt_add_executable(scancore_library_tests tst_library.cpp)
target_link_libraries(scancore_library_tests PRIVATE scancore_core Qt6::Test Qt6::CorePrivate)
add_test(NAME library COMMAND scancore_library_tests)

# Anime episodes saved for offline playback (HLS, files).
qt_add_executable(scancore_video_tests tst_video.cpp)
target_link_libraries(scancore_video_tests PRIVATE scancore_core Qt6::Test)
add_test(NAME video COMMAND scancore_video_tests)

# Extension repositories, trust, installs and extension sources.
qt_add_executable(scancore_extension_tests tst_extensions.cpp)
target_link_libraries(scancore_extension_tests PRIVATE scancore_core Qt6::Test Qt6::CorePrivate)
add_test(NAME extensions COMMAND scancore_extension_tests)
target_compile_definitions(scancore_extension_tests PRIVATE SCANCORE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

if(SCANCORE_HAVE_EXTENSION_HOST)
    # The host's own tests (Kotlin): the test extension loaded, converted and
    # run against the fixture site.
    add_test(NAME host COMMAND ${SCANCORE_GRADLE} -p ${SCANCORE_HOST_DIR} --console=plain -q :core:test :desktop:test)
    # The app's side: a fixture repository, install and trust, the host as a
    # helper process, the extension source and its images.
    qt_add_executable(scancore_host_tests tst_host.cpp)
    target_link_libraries(scancore_host_tests PRIVATE scancore_core Qt6::Test)
    add_dependencies(scancore_host_tests scancore_host)
    add_test(NAME host_process COMMAND scancore_host_tests)
    target_compile_definitions(scancore_host_tests PRIVATE SCANCORE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        SCANCORE_HOST_DEV_LIB="${SCANCORE_HOST_LIB_DIR}" SCANCORE_TESTEXT_APK="${SCANCORE_TESTEXT_APK}"
        SCANCORE_TESTANIME_APK="${SCANCORE_TESTANIME_APK}")
endif()

# Live source checks (network); run by hand or from the scheduled workflow.
qt_add_executable(scancore_probe probe.cpp)
target_link_libraries(scancore_probe PRIVATE scancore_core)
target_compile_definitions(scancore_probe PRIVATE SCANCORE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    SCANCORE_MAL_CLIENT_ID="${SCANCORE_MAL_CLIENT_ID}")
qt_add_resources(scancore_probe probe_lncrawl
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES ${PROJECT_SOURCE_DIR}/resources/lncrawl/sources.json ${PROJECT_SOURCE_DIR}/resources/lncrawl/bridge.py)

# LNReader plugins in QJSEngine: modules, web APIs, network, storage, limits.
qt_add_executable(scancore_plugin_tests tst_plugins.cpp)
target_link_libraries(scancore_plugin_tests PRIVATE scancore_core Qt6::Test)
add_test(NAME plugins COMMAND scancore_plugin_tests)
target_compile_definitions(scancore_plugin_tests PRIVATE SCANCORE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Console programs on Windows too, so CTest shows their output.
foreach(test scancore_tests scancore_catalogue_tests scancore_extension_tests scancore_plugin_tests scancore_probe scancore_host_tests)
    if(TARGET ${test})
        set_target_properties(${test} PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
endforeach()
