enable_testing()

add_executable(peer_connection_test
    WebRTC_test.cpp
)

add_executable(peer_connection_recorder_test
    WebRTC_recorder_test.cpp
)

add_executable(peer_connection_receiver_test
    WebRTC_receiver_test.cpp
)

add_executable(peer_connection_full_test
    WebRTC_full_test.cpp
)

# Get rtc source directory from FetchContent
FetchContent_GetProperties(rtc)
if(rtc_SOURCE_DIR)
    target_include_directories(peer_connection_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${rtc_SOURCE_DIR}/include
    )
    target_include_directories(peer_connection_recorder_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${rtc_SOURCE_DIR}/include
    )
    target_include_directories(peer_connection_receiver_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${rtc_SOURCE_DIR}/include
    )
    target_include_directories(peer_connection_full_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${rtc_SOURCE_DIR}/include
    )
else()
    target_include_directories(peer_connection_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_include_directories(peer_connection_recorder_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_include_directories(peer_connection_receiver_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_include_directories(peer_connection_full_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
endif()

target_link_libraries(peer_connection_test
    PRIVATE
    audio_sender
    audio_recorder
    datachannel
)

target_link_libraries(peer_connection_recorder_test
    PRIVATE
    audio_sender
    audio_recorder
    saving_worker
    datachannel
)

target_link_libraries(peer_connection_receiver_test
    PRIVATE
    audio_sender
    audio_recorder
    audio_receiver
    saving_worker
    datachannel
)

target_link_libraries(peer_connection_full_test
    PRIVATE
    audio_sender
    audio_recorder
    audio_receiver
    saving_worker
    datachannel
)

add_test(NAME PeerConnectionTest COMMAND peer_connection_test)
add_test(NAME PeerConnectionRecorderTest COMMAND peer_connection_recorder_test)
add_test(NAME PeerConnectionReceiverTest COMMAND peer_connection_receiver_test)
add_test(NAME PeerConnectionFullTest COMMAND peer_connection_full_test)