include(FetchContent)
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
        string(REPLACE "cmake_minimum_required (VERSION 3.1..3.18)"
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


add_library(wav_worker WavWorker.cpp)


target_include_directories(wav_worker
        PRIVATE
        ${sndfile_SOURCE_DIR}/include
)

target_link_libraries(wav_worker
        PUBLIC
        sndfile
)