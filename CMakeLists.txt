cmake_minimum_required(VERSION 3.16)
project(AudioRecorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

# Ensure libdatachannel target is available globally.
# AudioSender also fetches it, but if that directory isn't configured yet
# the target would be treated as a plain library name and fail to link.
include(FetchContent)
if(NOT TARGET datachannel)
    FetchContent_Declare(
        rtc
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(rtc)
endif()

# Google Test
if(NOT TARGET gtest)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()


add_subdirectory(src/SavingWorkers)
add_subdirectory(src/AudioRecorder)
add_subdirectory(src/AudioSender)
add_subdirectory(src/AudioReceiver)

enable_testing()
add_subdirectory(tests)

add_executable(AudioRecorderApp src/main.cpp)

target_include_directories(AudioRecorderApp
        PRIVATE
)

target_link_libraries(AudioRecorderApp
        PRIVATE
        saving_worker
        audio_recorder
        audio_sender
)
