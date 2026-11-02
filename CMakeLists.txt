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
set(CMAKE_C_EXTENSIONS OFF)

configure_file(
        "${PROJECT_SOURCE_DIR}/include/version.h.in"
        "${PROJECT_BINARY_DIR}/include/version.h"
)

add_executable(
        ${PROJECT_NAME}
        src/args.c
        src/diagnostics.c
        src/ir.c
        src/main.c
        src/parser.c
        src/vm.c
        src/lexer.c
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
        -Werror
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

include(CTest)

function(add_kage_test name)
    set(kage_file       "${PROJECT_SOURCE_DIR}/tests/${name}.kage")
    set(input_file      "${PROJECT_SOURCE_DIR}/tests/${name}.input")
    set(expected_file   "${PROJECT_SOURCE_DIR}/tests/${name}.output")

    if(NOT EXISTS "${kage_file}")
        message(FATAL_ERROR "add_kage_test: ${kage_file} not found")
    endif()

    if(NOT EXISTS "${input_file}")
        set(input_file "")
    endif()
    if(NOT EXISTS "${expected_file}")
        message(FATAL_ERROR "add_kage_test: ${expected_file} not found")
    endif()

    add_test(
            NAME ${name}
            COMMAND ${CMAKE_COMMAND}
            "-DKAGE=$<TARGET_FILE:${PROJECT_NAME}>"
            "-DFILE=${kage_file}"
            "-DEXPECTED=${expected_file}"
            "-DINPUT=${input_file}"
            "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/${name}.actual"
            -P "${PROJECT_SOURCE_DIR}/tests/run_test.cmake"
    )
endfunction()

if(BUILD_TESTING)
    foreach(test_case IN ITEMS
            hi
            echo_char
            store_lit
            copy_register
            copy_zero_reg
            arith_add
            arith_sub
            arith_mul
            arith_div
            arith_mod
            div_zero
            jmp
            cond_jmp_taken
            cond_jmp_skip
            loop
            dec_print
            char_input
            zero_reg
            overflow_add
            overflow_sub
            overflow_mul
            overflow_div
            overflow_mod
            min_integer
            max_integer
    )
        add_kage_test(${test_case})
    endforeach()

    function(add_kage_error_test name expected_error)
        add_test(
                NAME ${name}
                COMMAND ${CMAKE_COMMAND}
                "-DKAGE=$<TARGET_FILE:${PROJECT_NAME}>"
                "-DFILE=${PROJECT_SOURCE_DIR}/tests/errors/${name}.kage"
                "-DEXPECTED_ERROR=${expected_error}"
                -P "${PROJECT_SOURCE_DIR}/tests/run_error_test.cmake"
        )
    endfunction()

    add_kage_error_test(integer_overflow "outside the int32 range")
    add_kage_error_test(unexpected_character "Unexpected character")
    add_kage_error_test(word_too_long "maximum length of 31 characters")
    add_kage_error_test(missing_operand "expected NUM")
    add_kage_error_test(negative_register "register address must be non-negative")
    add_kage_error_test(duplicate_label "duplicate label")
    add_kage_error_test(undefined_label "undefined label")
    add_test(
            NAME multiple_compiler_errors
            COMMAND ${CMAKE_COMMAND}
            "-DKAGE=$<TARGET_FILE:${PROJECT_NAME}>"
            "-DFILE=${PROJECT_SOURCE_DIR}/tests/errors/multiple_errors.kage"
            -P "${PROJECT_SOURCE_DIR}/tests/run_diagnostics_test.cmake"
    )
    add_test(
            NAME instruction_limit
            COMMAND ${CMAKE_COMMAND}
            "-DKAGE=$<TARGET_FILE:${PROJECT_NAME}>"
            "-DFILE=${PROJECT_SOURCE_DIR}/tests/errors/instruction_limit.kage"
            "-DEXPECTED_ERROR=maximum instruction count"
            -P "${PROJECT_SOURCE_DIR}/tests/run_instruction_limit_test.cmake"
    )
    foreach(io_mode IN ITEMS input output)
        add_test(
                NAME ${io_mode}_file_error
                COMMAND ${CMAKE_COMMAND}
                "-DKAGE=$<TARGET_FILE:${PROJECT_NAME}>"
                "-DFILE=${PROJECT_SOURCE_DIR}/tests/errors/io_failure.kage"
                "-DIO_MODE=${io_mode}"
                "-DIO_PATH=${CMAKE_CURRENT_BINARY_DIR}/missing-${io_mode}-directory/output"
                -P "${PROJECT_SOURCE_DIR}/tests/run_io_error_test.cmake"
        )
    endforeach()
    add_test(
            NAME stdout_output
            COMMAND ${CMAKE_COMMAND}
            "-DKAGE=$<TARGET_FILE:${PROJECT_NAME}>"
            -P "${PROJECT_SOURCE_DIR}/tests/run_stdout_test.cmake"
    )
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

message(STATUS "Project      : ${PROJECT_NAME}")
message(STATUS "Version      : ${PROJECT_VERSION}")
message(STATUS "Compiler     : ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard   : C${CMAKE_C_STANDARD}")
message(STATUS "Author       : ${PROJECT_AUTHOR}")
message(STATUS "Contributor  : ${PROJECT_CONTRIBUTOR}")
message(STATUS "Repository   : ${PROJECT_REPOSITORY}")
