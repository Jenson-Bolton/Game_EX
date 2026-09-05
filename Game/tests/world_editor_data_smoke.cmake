# Runs one loaded-data editor smoke and requires both success and load evidence.

if(NOT DEFINED WORLD_EDITOR OR NOT DEFINED WORLD_PACKAGE OR NOT DEFINED RENDERER)
    message(FATAL_ERROR "WORLD_EDITOR, WORLD_PACKAGE, and RENDERER are required")
endif()

execute_process(
    COMMAND
        "${WORLD_EDITOR}"
        "--world=${WORLD_PACKAGE}"
        "--renderer=${RENDERER}"
        --quit-after-ms=250
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR
        "${RENDERER} loaded-data smoke failed with exit ${result}\n"
        "stdout:\n${output}\n"
        "stderr:\n${error_output}"
    )
endif()
if(NOT output MATCHES "World terrain:.*source=2x2 display=2x2 valid=3 invalid=1")
    message(FATAL_ERROR
        "${RENDERER} loaded-data smoke omitted package visualization evidence\n"
        "stdout:\n${output}"
    )
endif()

message(STATUS "${RENDERER} loaded-data world-editor smoke passed")
