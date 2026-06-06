# Called by CTest via cmake -P.
#   KAGE        path to kage binary
#   FILE        path to .kage source file
#   EXPECTED    expected stdout output
#   INPUT_FILE  path to file used as stdin, or empty string for no input

if(INPUT_FILE AND EXISTS "${INPUT_FILE}")
    execute_process(
            COMMAND         ${KAGE} ${FILE}
            INPUT_FILE      ${INPUT_FILE}
            OUTPUT_VARIABLE actual
            ERROR_VARIABLE  ignored
    )
else()
    execute_process(
            COMMAND         ${KAGE} ${FILE}
            OUTPUT_VARIABLE actual
            ERROR_VARIABLE  ignored
    )
endif()

string(STRIP "${actual}"   actual)
string(STRIP "${EXPECTED}" expected_clean)

if(NOT actual STREQUAL expected_clean)
    message(FATAL_ERROR
            "Test FAILED: ${FILE}\n"
            "  expected: [${expected_clean}]\n"
            "  got:      [${actual}]"
    )
endif()
