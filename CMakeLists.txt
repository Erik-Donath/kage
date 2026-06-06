cmake_minimum_required(VERSION 3.20)

set(PROJECT_AUTHOR      "Erik Donath")
set(PROJECT_CONTRIBUTOR "Erik Donath")
set(PROJECT_REPOSITORY  "https://github.com/Erik-Donath/kage")
set(PROJECT_HOMEPAGE    "https://www.erikdonath.de")

project(
        kage
        VERSION 0.3.0
        DESCRIPTION "Kage Programming Language"
        LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

configure_file(
        "${PROJECT_SOURCE_DIR}/include/version.h.in"
        "${PROJECT_BINARY_DIR}/include/version.h"
)

file(
        GLOB_RECURSE KAGE_SOURCES
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.c"
)

add_executable(
        ${PROJECT_NAME}
        ${KAGE_SOURCES}
)

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
    )
endif()

enable_testing()

function(add_kage_test name expected)
    set(kage_file   "${PROJECT_SOURCE_DIR}/tests/${name}.kage")
    set(input_file  "${PROJECT_SOURCE_DIR}/tests/${name}.input")

    if(NOT EXISTS "${kage_file}")
        message(WARNING "add_kage_test: ${kage_file} not found, skipping '${name}'")
        return()
    endif()

    # Pass empty string if no input file exists
    if(NOT EXISTS "${input_file}")
        set(input_file "")
    endif()

    add_test(
            NAME ${name}
            COMMAND ${CMAKE_COMMAND}
            -DKAGE=$<TARGET_FILE:kage>
            -DFILE=${kage_file}
            -DEXPECTED=${expected}
            -DINPUT_FILE=${input_file}
            -P "${PROJECT_SOURCE_DIR}/tests/run_test.cmake"
    )
endfunction()

add_kage_test(hi        "Hi")
add_kage_test(echo_char "A")

message(STATUS "Project      : ${PROJECT_NAME}")
message(STATUS "Version      : ${PROJECT_VERSION}")
message(STATUS "Compiler     : ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard   : C${CMAKE_C_STANDARD}")
message(STATUS "Author       : ${PROJECT_AUTHOR}")
message(STATUS "Contributor  : ${PROJECT_CONTRIBUTOR}")
message(STATUS "Repository   : ${PROJECT_REPOSITORY}")
