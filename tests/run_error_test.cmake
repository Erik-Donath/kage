# Variables: KAGE, FILE, EXPECTED_ERROR
execute_process(
    COMMAND "${KAGE}" "${FILE}" --max-steps 10000
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    string(FIND "${stderr}" "${EXPECTED_ERROR}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Expected error containing [${EXPECTED_ERROR}] but saw stderr: [${stderr}]")
    endif()
    return()
endif()
message(FATAL_ERROR "Expected failure for ${FILE}, but program exited successfully
stdout: [${stdout}]
stderr: [${stderr}]")
