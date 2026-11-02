# Variables: KAGE, FILE
execute_process(
    COMMAND "${KAGE}" "${FILE}" --max-steps 10000
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(result EQUAL 0)
    message(FATAL_ERROR "Expected compiler diagnostics for ${FILE}, but execution succeeded")
endif()
string(REGEX REPLACE "\n" ";" diag_lines "${stderr}")
set(count 0)
foreach(line IN LISTS diag_lines)
    if(line MATCHES "(LexerError|ParseError|LabelError)")
        math(EXPR count "${count} + 1")
    endif()
endforeach()
if(count LESS 2)
    message(FATAL_ERROR "Expected multiple diagnostics for ${FILE}, got: [${stderr}]")
endif()
