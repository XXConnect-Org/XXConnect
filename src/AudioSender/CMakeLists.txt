include(FetchContent)

# libdatachannel
FetchContent_Declare(
        rtc
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG master
)

FetchContent_MakeAvailable(rtc)

add_library(audio_sender
        AudioSender.hpp
        AudioSender.cpp
)

target_link_libraries(audio_sender
        PUBLIC
        rtc
        audio_recorder
        saving_worker
)

target_include_directories(audio_sender
        PUBLIC
        ${rtc_SOURCE_DIR}/include
)


