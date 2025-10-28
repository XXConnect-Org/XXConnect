include(FetchContent)

# RtAudio
FetchContent_Declare(
        rtaudio
        GIT_REPOSITORY https://github.com/thestk/rtaudio.git
        GIT_TAG 6.0.1
)

FetchContent_MakeAvailable(rtaudio)
