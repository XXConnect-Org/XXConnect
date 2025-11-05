include(FetchContent)

FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.19.5
)

set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE)
# set(NO_MEDIA ON CACHE BOOL "" FORCE)  
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)

# FetchContent_Declare(json_target
#     GIT_REPOSITORY https://github.com/nlohmann/json
#     GIT_TAG v3.12.0
# )
# FetchContent_MakeAvailable(json_target)

FetchContent_MakeAvailable(libdatachannel)
add_library(client STATIC client_connection_manager.cpp client.cpp)

target_include_directories(client PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${libdatachannel_SOURCE_DIR}/include
    PRIVATE ${libdatachannel_BINARY_DIR}
)

target_link_libraries(client PUBLIC LibDataChannel::LibDataChannel)
target_link_libraries(client PUBLIC nlohmann_json::nlohmann_json)