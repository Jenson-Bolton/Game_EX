# Compiles the existing synthetic staging manifest into a test-owned package.

if(NOT DEFINED WORLD_COMPILER OR NOT DEFINED MANIFEST OR NOT DEFINED OUTPUT_PACKAGE)
    message(FATAL_ERROR "WORLD_COMPILER, MANIFEST, and OUTPUT_PACKAGE are required")
endif()

get_filename_component(output_directory "${OUTPUT_PACKAGE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(REMOVE "${OUTPUT_PACKAGE}")

execute_process(
    COMMAND "${WORLD_COMPILER}" compile --manifest "${MANIFEST}" --output "${OUTPUT_PACKAGE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT "${result}" STREQUAL "0" OR NOT EXISTS "${OUTPUT_PACKAGE}")
    message(FATAL_ERROR
        "Could not compile the world-editor fixture (exit ${result})\n"
        "stdout:\n${output}\n"
        "stderr:\n${error_output}"
    )
endif()

message(STATUS "World-editor fixture compiled: ${OUTPUT_PACKAGE}")
