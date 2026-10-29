# Variables: KAGE, FILE, EXPECTED_ERROR
execute_process(
    COMMAND "${KAGE}" "${FILE}" --max-steps 1
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    string(FIND "${stderr}" "${EXPECTED_ERROR}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Expected stderr to contain [${EXPECTED_ERROR}], got: [${stderr}]")
    endif()
    return()
endif()
message(FATAL_ERROR "Expected max-instruction failure, but program exited successfully
stdout: [${stdout}]
stderr: [${stderr}]")
