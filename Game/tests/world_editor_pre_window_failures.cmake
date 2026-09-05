# Verifies missing and corrupt packages fail before renderer selection begins.

if(NOT DEFINED WORLD_EDITOR OR NOT DEFINED VALID_PACKAGE OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "WORLD_EDITOR, VALID_PACKAGE, and TEST_ROOT are required")
endif()
if(NOT EXISTS "${VALID_PACKAGE}")
    message(FATAL_ERROR "Fixture package is missing: ${VALID_PACKAGE}")
endif()

file(MAKE_DIRECTORY "${TEST_ROOT}")
set(missing_package "${TEST_ROOT}/missing.gexworld")
set(corrupt_package "${TEST_ROOT}/corrupt.gexworld")
file(REMOVE "${missing_package}" "${corrupt_package}")
file(COPY_FILE "${VALID_PACKAGE}" "${corrupt_package}")
file(APPEND "${corrupt_package}" "corruption")

function(require_pre_window_failure package_path expected_pattern context)
    execute_process(
        COMMAND
            "${WORLD_EDITOR}"
            "--world=${package_path}"
            --renderer=opengl
            --quit-after-ms=0
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    set(combined_output "${output}\n${error_output}")
    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "${context}: editor unexpectedly succeeded")
    endif()
    if(NOT combined_output MATCHES "${expected_pattern}")
        message(FATAL_ERROR
            "${context}: expected package diagnostic was absent\n${combined_output}"
        )
    endif()
    if(combined_output MATCHES "Trying renderer:")
        message(FATAL_ERROR
            "${context}: renderer selection began before package failure\n${combined_output}"
        )
    endif()
endfunction()

require_pre_window_failure("${missing_package}" "Cannot determine package size" "missing package")
require_pre_window_failure("${corrupt_package}" "integrity check failed" "corrupt package")
