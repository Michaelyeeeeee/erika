cmake_minimum_required(VERSION 3.16)

project(erika
    VERSION 1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(
    ALSA
    REQUIRED
    IMPORTED_TARGET
    alsa
)

# ------------------------------------------------------------
# whisper.cpp
# ------------------------------------------------------------

#
# Erika only needs the Whisper library.
# Do not build all of whisper.cpp's examples/tests/server.
#
set(
    WHISPER_BUILD_EXAMPLES
    OFF
    CACHE BOOL ""
    FORCE
)

set(
    WHISPER_BUILD_TESTS
    OFF
    CACHE BOOL ""
    FORCE
)

set(
    WHISPER_BUILD_SERVER
    OFF
    CACHE BOOL ""
    FORCE
)

#
# Build the copy located in:
#
#   erika/whisper.cpp/
#
add_subdirectory(
    whisper.cpp
    whisper-build
)

# ------------------------------------------------------------
# Erika
# ------------------------------------------------------------

add_executable(erika
    src/main.cpp
    src/audio_device.cpp
    src/audio_capture.cpp
    src/wake_word.cpp
    src/transcriber.cpp
    src/command_handler.cpp
    src/actions.cpp
    src/feedback.cpp
)

target_include_directories(erika
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/vosk/vosk-linux-aarch64-0.3.45
)

target_link_directories(erika
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vosk/vosk-linux-aarch64-0.3.45
)

target_link_libraries(erika
    PRIVATE
        PkgConfig::ALSA
        vosk
        whisper
)

set_target_properties(erika PROPERTIES
    BUILD_RPATH
        "${CMAKE_CURRENT_SOURCE_DIR}/vosk/vosk-linux-aarch64-0.3.45"
)