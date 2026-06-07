# Variables (passed via -D):
#   KAGE        path to kage binary
#   FILE        path to .kage source file
#   EXPECTED    path to .output file
#   INPUT       path to .input file, or empty string

if(INPUT AND EXISTS "${INPUT}")
    execute_process(
            COMMAND ${KAGE} ${FILE} -i ${INPUT} -o ${FILE}.result --max-steps 10000
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
    )
else()
    execute_process(
            COMMAND ${KAGE} ${FILE} -o ${FILE}.result --max-steps 10000
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
    )
endif()

message("result=${result}")
message("stdout=${stdout}")
message("stderr=${stderr}")

file(READ "${FILE}.result"  actual)
file(READ "${EXPECTED}"     expected)

string(STRIP "${actual}"   actual)
string(STRIP "${expected}" expected)

file(REMOVE "${FILE}.result")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR
            "Test FAILED: ${FILE}\n"
            "  expected: [${expected}]\n"
            "  got:      [${actual}]"
    )
endif()
