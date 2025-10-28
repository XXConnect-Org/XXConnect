cmake_minimum_required(VERSION 3.16)
project(AudioRecorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

include(FetchContent)

# RtAudio
FetchContent_Declare(
        rtaudio
        GIT_REPOSITORY https://github.com/thestk/rtaudio.git
        GIT_TAG 6.0.1
)

# libsndfile
FetchContent_Declare(
        sndfile
        GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
        GIT_TAG 1.2.2
)

# Патчим libsndfile
macro(patch_sndfile_cmake)
    if(EXISTS ${sndfile_SOURCE_DIR}/CMakeLists.txt)
        file(READ ${sndfile_SOURCE_DIR}/CMakeLists.txt SNDFILE_CMAKE)
        string(REPLACE "cmake_minimum_required(VERSION 2.8.12)"
                "cmake_minimum_required(VERSION 3.5)"
                SNDFILE_CMAKE ${SNDFILE_CMAKE})
        file(WRITE ${sndfile_SOURCE_DIR}/CMakeLists.txt "${SNDFILE_CMAKE}")
        message(STATUS "Patched libsndfile CMakeLists.txt")
    endif()
endmacro()

# Загружаем и патчим libsndfile
FetchContent_GetProperties(sndfile)
if(NOT sndfile_POPULATED)
    FetchContent_Populate(sndfile)
    patch_sndfile_cmake()
    add_subdirectory(${sndfile_SOURCE_DIR} ${sndfile_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_MakeAvailable(rtaudio)

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