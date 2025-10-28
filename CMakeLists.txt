cmake_minimum_required(VERSION 3.16)
project(AudioRecorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

add_subdirectory(AudioRecorder)
add_subdirectory(WavWorker)
add_executable(AudioRecorderApp src/main.cpp)

target_include_directories(AudioRecorderApp
        PRIVATE
        ${rtaudio_SOURCE_DIR}/include
        ${sndfile_SOURCE_DIR}/include
)

target_link_libraries(AudioRecorderApp
        PRIVATE
        rtaudio
        sndfile
        wav_worker
)

# Фреймворки macOS
find_library(AUDIO_TOOLBOX AudioToolbox)
find_library(CORE_AUDIO CoreAudio)
find_library(CORE_FOUNDATION CoreFoundation)
find_library(CORE_SERVICES CoreServices)
find_library(AUDIO_UNIT AudioUnit)

target_link_libraries(AudioRecorderApp PRIVATE
        ${AUDIO_TOOLBOX}
        ${CORE_AUDIO}
        ${CORE_FOUNDATION}
        ${CORE_SERVICES}
        ${AUDIO_UNIT}
)

set_target_properties(AudioRecorderApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)