cmake_minimum_required(VERSION 3.14)
project(VoiceCallApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Build tests" ON)

include(FetchContent)

FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.19.5
)

set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE)
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libdatachannel)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

find_package(websocketpp REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTAUDIO REQUIRED rtaudio)

add_library(voicecall STATIC
    src/WebRTCCall.cpp
    src/SignalingClient.cpp
    src/AudioProcessor.cpp
)

target_include_directories(voicecall PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${libdatachannel_SOURCE_DIR}/include
    PRIVATE ${libdatachannel_BINARY_DIR}
)

target_link_libraries(voicecall PUBLIC
    LibDataChannel::LibDataChannel
    nlohmann_json::nlohmann_json
    ${RTAUDIO_LIBRARIES}
    ${websocketpp_LIBRARIES}
)

add_executable(voice_call_app src/main.cpp)
target_link_libraries(voice_call_app PRIVATE voicecall)
