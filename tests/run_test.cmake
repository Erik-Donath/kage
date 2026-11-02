# Variables (passed via -D):
#   KAGE        path to kage binary
#   FILE        path to .kage source file
#   EXPECTED    path to .output file
#   INPUT       path to .input file, or empty string
#   ACTUAL      path to a build-directory output file

if(INPUT)
    execute_process(
            COMMAND "${KAGE}" "${FILE}" -i "${INPUT}" -o "${ACTUAL}" --max-steps 10000
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
    )
else()
    execute_process(
            COMMAND "${KAGE}" "${FILE}" -o "${ACTUAL}" --max-steps 10000
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
    )
endif()

if(NOT result EQUAL 0)
    file(REMOVE "${ACTUAL}")
    message(FATAL_ERROR
            "Kage exited with ${result}\n"
            "stdout: ${stdout}\n"
            "stderr: ${stderr}")
endif()

if(NOT stdout STREQUAL "")
    file(REMOVE "${ACTUAL}")
    message(FATAL_ERROR "Unexpected standard output: [${stdout}]")
endif()

if(NOT EXISTS "${ACTUAL}")
    message(FATAL_ERROR "Kage did not create its output file")
endif()

file(READ "${ACTUAL}" actual)
file(READ "${EXPECTED}" expected)

file(REMOVE "${ACTUAL}")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR
            "Test FAILED: ${FILE}\n"
            "  expected: [${expected}]\n"
            "  got:      [${actual}]\n"
            "  stdout:   [${stdout}]\n"
            "  stderr:   [${stderr}]"
    )
endif()
