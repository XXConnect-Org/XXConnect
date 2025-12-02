cmake_minimum_required(VERSION 3.16)
project(AudioRecorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

add_subdirectory(src/SavingWorkers)
add_subdirectory(src/AudioRecorder)
add_subdirectory(src/AudioSender)
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
