cmake_minimum_required(VERSION 3.20)
project(SpeechToTelop VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tests — pure-logic files, no OBS dependency
enable_testing()
find_package(GTest 1.14 REQUIRED)

add_executable(SpeechToTelopTests
    tests/test-ring-buffer.cpp
    tests/test-text-buffer.cpp
    tests/test-animation.cpp
    tests/test-pipeline-registry.cpp
    src/text-buffer.cpp
    src/telop-animation.cpp
    src/pipeline-registry.cpp
    src/pipeline.cpp
)
target_include_directories(SpeechToTelopTests PRIVATE src)
target_link_libraries(SpeechToTelopTests GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(SpeechToTelopTests)

# OBS plugin — only built when libobs is available
find_package(libobs QUIET)
find_package(CURL QUIET)

if(NOT libobs_FOUND)
    message(STATUS "libobs not found — skipping plugin target (tests still build)")
    return()
endif()
if(NOT CURL_FOUND)
    message(STATUS "CURL not found — skipping plugin target")
    return()
endif()

# whisper.cpp — fetched at configure time
include(FetchContent)
FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG        v1.7.3
    GIT_SHALLOW    TRUE
)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(whisper)

# Plugin target
add_library(SpeechToTelop MODULE
    src/plugin-main.cpp
    src/audio-resampler.cpp
    src/pipeline.cpp
    src/pipeline-registry.cpp
    src/stt-engine.cpp
    src/stt-whisper.cpp
    src/stt-cloud.cpp
    src/model-downloader.cpp
    src/text-buffer.cpp
    src/audio-filter.cpp
    src/telop-source.cpp
    src/telop-renderer.cpp
    src/telop-animation.cpp
)
target_include_directories(SpeechToTelop PRIVATE src)
target_link_libraries(SpeechToTelop
    PRIVATE OBS::libobs CURL::libcurl whisper
)
if(APPLE)
    target_link_libraries(SpeechToTelop PRIVATE
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
    )
endif()
set_target_properties(SpeechToTelop PROPERTIES PREFIX "")
if(APPLE)
    set_target_properties(SpeechToTelop PROPERTIES SUFFIX ".dylib")
endif()

# Install locale data
install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_PREFIX}/data/obs-plugins/SpeechToTelop")
