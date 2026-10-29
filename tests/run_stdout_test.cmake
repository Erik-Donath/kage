# Variables: KAGE
execute_process(
    COMMAND "${KAGE}" -e "1 yami 72 2 yami 105 3 yami 10 rei 1 rei 2 rei 3 rei 0"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Kage failed: stdout=[${stdout}] stderr=[${stderr}]")
endif()
if(NOT stdout STREQUAL "Hi
")
    message(FATAL_ERROR "Expected stdout [Hi\n], got [${stdout}]")
endif()
