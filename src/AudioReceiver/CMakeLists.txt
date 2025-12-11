include(FetchContent)

# libdatachannel
FetchContent_Declare(
        rtc
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG master
)

FetchContent_MakeAvailable(rtc)

add_library(audio_receiver
        AudioReceiver.hpp
        AudioReceiver.cpp
)

target_link_libraries(audio_receiver
        PUBLIC
        datachannel
)

target_include_directories(audio_receiver
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${rtc_SOURCE_DIR}/include
)

