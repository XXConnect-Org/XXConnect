cmake_minimum_required(VERSION 3.16)
project(AudioRecorderLibrary)

include(FetchContent)

# RtAudio - современная альтернатива PortAudio
FetchContent_Declare(
        rtaudio
        GIT_REPOSITORY https://github.com/thestk/rtaudio.git
        GIT_TAG 6.0.1
)

FetchContent_MakeAvailable(rtaudio)

# Создание статической библиотеки
add_library(AudioRecorder STATIC
        /AudioRecorder.cpp
        src/WavWriter.cpp
)

# Настройка include директорий
target_include_directories(AudioRecorder
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${rtaudio_SOURCE_DIR}/include
)

# Линковка с зависимостями
target_link_libraries(AudioRecorder
        PUBLIC
        rtaudio
)

# Фреймворки macOS для аудио
if(APPLE)
    find_library(AUDIO_TOOLBOX AudioToolbox)
    find_library(CORE_AUDIO CoreAudio)
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(CORE_SERVICES CoreServices)
    find_library(AUDIO_UNIT AudioUnit)
    find_library(CARBON Carbon)

    target_link_libraries(AudioRecorder
            PRIVATE
            ${AUDIO_TOOLBOX}
            ${CORE_AUDIO}
            ${CORE_FOUNDATION}
            ${CORE_SERVICES}
            ${AUDIO_UNIT}
            ${CARBON}
    )
endif()

set_target_properties(AudioRecorder PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)