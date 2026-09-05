# Dependency-free end-to-end CLI contract tests for the world compiler.

if(NOT DEFINED WORLD_COMPILER OR NOT DEFINED TEST_ROOT OR NOT DEFINED FIXTURE_DIR)
    message(FATAL_ERROR "WORLD_COMPILER, TEST_ROOT, and FIXTURE_DIR are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/stage")
file(COPY "${FIXTURE_DIR}/" DESTINATION "${TEST_ROOT}/stage")

function(require_result actual expected context)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${context}: expected exit ${expected}, observed ${actual}")
    endif()
endfunction()

execute_process(
    COMMAND "${WORLD_COMPILER}" --help
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "0" "--help")
if(NOT output MATCHES "game_ex_world_compiler compile")
    message(FATAL_ERROR "--help did not describe compile")
endif()

execute_process(
    COMMAND "${WORLD_COMPILER}" --version
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
)
require_result("${result}" "0" "--version")
if(NOT output MATCHES "world_format=0\\.2")
    message(FATAL_ERROR "--version did not report world format 0.2")
endif()

execute_process(
    COMMAND "${WORLD_COMPILER}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "2" "missing command")
if(NOT error_output MATCHES "usage:")
    message(FATAL_ERROR "Missing command did not report usage")
endif()

set(manifest "${TEST_ROOT}/stage/manifest.gexstage")
set(package "${TEST_ROOT}/stage/result.gexworld")
execute_process(
    COMMAND "${WORLD_COMPILER}" validate --manifest "${manifest}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "0" "validate")
if(NOT output MATCHES "status=valid" OR NOT output MATCHES "origin_z=250\\.25")
    message(FATAL_ERROR "validate summary omitted required fields")
endif()

execute_process(
    COMMAND "${WORLD_COMPILER}" compile --manifest "${manifest}" --output "${package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "0" "compile")
if(NOT EXISTS "${package}" OR NOT output MATCHES "status=compiled")
    message(FATAL_ERROR "compile did not publish and report a package")
endif()

execute_process(
    COMMAND "${WORLD_COMPILER}" inspect --package "${package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "0" "inspect")
if(NOT output MATCHES "tile_max_x=-741998\\.125"
   OR NOT output MATCHES "valid_absolute_z_max=254\\.25"
   OR NOT output MATCHES "provenance.attribution=Jenson Bolton / Game_EX"
   OR NOT output MATCHES "staged.heights.sha256=49a790b3")
    message(FATAL_ERROR "inspect summary omitted bounds, absolute height, or provenance")
endif()

execute_process(
    COMMAND "${WORLD_COMPILER}" compile --manifest "${manifest}" --output "${package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "6" "existing output")

file(COPY "${FIXTURE_DIR}/" DESTINATION "${TEST_ROOT}/bad_manifest")
file(APPEND "${TEST_ROOT}/bad_manifest/manifest.gexstage" "unknown.key=value\n")
execute_process(
    COMMAND "${WORLD_COMPILER}" validate
        --manifest "${TEST_ROOT}/bad_manifest/manifest.gexstage"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "3" "invalid manifest")

file(COPY "${FIXTURE_DIR}/" DESTINATION "${TEST_ROOT}/tampered_stage")
file(APPEND "${TEST_ROOT}/tampered_stage/heights.txt" "9")
execute_process(
    COMMAND "${WORLD_COMPILER}" validate
        --manifest "${TEST_ROOT}/tampered_stage/manifest.gexstage"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "4" "tampered staged input")

file(APPEND "${package}" "x")
execute_process(
    COMMAND "${WORLD_COMPILER}" inspect --package "${package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "5" "corrupt package")

execute_process(
    COMMAND "${WORLD_COMPILER}" inspect --package "${TEST_ROOT}/missing.gexworld"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
require_result("${result}" "6" "missing package")

file(REMOVE_RECURSE "${TEST_ROOT}")
