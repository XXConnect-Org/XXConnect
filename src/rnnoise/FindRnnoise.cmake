# rnnoise/FindRNNoise.cmake
find_path(RNNOISE_INCLUDE_DIR
        NAMES rnnoise.h
        PATHS ${CMAKE_INSTALL_PREFIX}/include
        /usr/local/include
        /usr/include
)

find_library(RNNOISE_LIBRARY
        NAMES rnnoise
        PATHS ${CMAKE_INSTALL_PREFIX}/lib
        /usr/local/lib
        /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RNNoise DEFAULT_MSG
        RNNOISE_LIBRARY
        RNNOISE_INCLUDE_DIR
)

if(RNNOISE_FOUND)
    set(RNNOISE_LIBRARIES ${RNNOISE_LIBRARY})
    set(RNNOISE_INCLUDE_DIRS ${RNNOISE_INCLUDE_DIR})
endif()

mark_as_advanced(RNNOISE_INCLUDE_DIR RNNOISE_LIBRARY)