# Variables: KAGE, FILE, IO_MODE, IO_PATH
set(args "${FILE}")
if(IO_MODE STREQUAL "input")
    list(APPEND args -i "${IO_PATH}")
else()
    list(APPEND args -o "${IO_PATH}")
endif()
execute_process(
    COMMAND "${KAGE}" ${args} --max-steps 10000
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(result EQUAL 0)
    message(FATAL_ERROR "Expected IO failure for ${IO_MODE}, but execution succeeded
stdout: [${stdout}]
stderr: [${stderr}]")
endif()
if(IO_MODE STREQUAL "input")
    string(FIND "${stderr}" "cannot open input file" found)
else()
    string(FIND "${stderr}" "cannot open output file" found)
endif()
if(found EQUAL -1)
    message(FATAL_ERROR "Expected IO error for ${IO_MODE}, got: [${stderr}]")
endif()
