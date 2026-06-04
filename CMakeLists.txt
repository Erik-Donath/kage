cmake_minimum_required(VERSION 3.20)

set(PROJECT_AUTHOR      "Erik Donath")
set(PROJECT_CONTRIBUTOR "Erik Donath")
set(PROJECT_REPOSITORY  "https://github.com/Erik-Donath/kage")
set(PROJECT_HOMEPAGE    "https://www.erikdonath.de")

project(
        kage
        VERSION 0.1.0
        DESCRIPTION "Kage Programming Language"
        LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

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

message(STATUS "Project      : ${PROJECT_NAME}")
message(STATUS "Version      : ${PROJECT_VERSION}")
message(STATUS "Compiler     : ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard   : C${CMAKE_C_STANDARD}")
message(STATUS "Author       : ${PROJECT_AUTHOR}")
message(STATUS "Contributor  : ${PROJECT_CONTRIBUTOR}")
message(STATUS "Repository   : ${PROJECT_REPOSITORY}")
