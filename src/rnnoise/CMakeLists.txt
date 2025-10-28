# src/rnnoise/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Скачиваем RNNoise если нужно
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/src/denoise.c)
    find_package(Git REQUIRED)
    message(STATUS "Cloning RNNoise...")
    execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 https://github.com/xiph/rnnoise.git external
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_CLONE_RESULT
    )
    if(NOT GIT_CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone RNNoise")
    endif()
    message(STATUS "RNNoise cloned successfully")
endif()

# Создаем config.h
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config.h "
#ifndef CONFIG_H
#define CONFIG_H

#define HAVE_STDINT_H 1
#define HAVE_LRT 0
#define HAVE___BUILTIN_CPU_SUPPORTS 1

#endif // CONFIG_H
")

# Компилируем RNNoise
add_library(rnnoise STATIC
        external/src/denoise.c
        external/src/rnn.c
        external/src/rnn_data.c
        external/src/pitch.c
        external/src/kiss_fft.c
        external/src/celt_lpc.c
)

# Include директории для RNNoise
target_include_directories(rnnoise PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/include
        ${CMAKE_CURRENT_BINARY_DIR}  # для config.h
)

target_compile_definitions(rnnoise PRIVATE HAVE_CONFIG_H)

# Компиляционные флаги
if(APPLE)
    target_compile_options(rnnoise PRIVATE
            -Wno-shorten-64-to-32
            -Wno-unused-parameter
            -Wno-deprecated-declarations
    )
endif()

message(STATUS "RNNoise library ready")