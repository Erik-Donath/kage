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

function(add_kage_test name)
    set(kage_file       "${PROJECT_SOURCE_DIR}/tests/${name}.kage")
    set(input_file      "${PROJECT_SOURCE_DIR}/tests/${name}.input")
    set(expected_file   "${PROJECT_SOURCE_DIR}/tests/${name}.output")

    if(NOT EXISTS "${kage_file}")
        message(WARNING "add_kage_test: ${kage_file} not found, skipping '${name}'")
        return()
    endif()

    # Pass empty string if no input file exists
    if(NOT EXISTS "${input_file}")
        set(input_file "")
    endif()
    # Pass empty string if no expected file exists
    if(NOT EXISTS "${expected_file}")
        set(expected_file "")
    endif()

    add_test(
            NAME ${name}
            COMMAND ${CMAKE_COMMAND}
            -DKAGE=$<TARGET_FILE:kage>
            -DFILE=${kage_file}
            -DEXPECTED=${expected_file}
            -DINPUT=${input_file}
            -P "${PROJECT_SOURCE_DIR}/tests/run_test.cmake"
    )
endfunction()

# Existing tests
add_kage_test(hi)
add_kage_test(echo_char)
add_kage_test(store_lit)       # yami  - store literal
add_kage_test(copy_register)   # Yami  - copy register
add_kage_test(arith_add)       # YAMI + - addition
add_kage_test(arith_sub)       # YAMI - - subtraction
add_kage_test(arith_mul)       # YAMI * - multiplication
add_kage_test(arith_div)       # YAMI / - division
add_kage_test(arith_mod)       # YAMI % - modulo
add_kage_test(div_zero)        # division and modulo by zero produce 0
add_kage_test(jmp)             # Hikari - unconditional jump
add_kage_test(cond_jmp_taken)  # HIKARI - conditional jump taken (nonzero)
add_kage_test(cond_jmp_skip)   # HIKARI - conditional jump not taken (zero)
add_kage_test(loop)            # hikari + HIKARI - loop with countdown
add_kage_test(dec_print)       # REI - decimal print
add_kage_test(char_input)      # Rei - character read
add_kage_test(zero_reg)        # r0 reads as 0, writes are ignored

message(STATUS "Project      : ${PROJECT_NAME}")
message(STATUS "Version      : ${PROJECT_VERSION}")
message(STATUS "Compiler     : ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard   : C${CMAKE_C_STANDARD}")
message(STATUS "Author       : ${PROJECT_AUTHOR}")
message(STATUS "Contributor  : ${PROJECT_CONTRIBUTOR}")
message(STATUS "Repository   : ${PROJECT_REPOSITORY}")
