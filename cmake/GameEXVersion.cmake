# Game_EX project version authority.
#
# Every independently configurable subproject includes this file before its
# project() declaration. Increment this value once for each reported release.
include_guard(DIRECTORY)

set(GAMEEX_VERSION "0.1.5")

function(gameex_apply_version target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot apply the Game_EX version to missing target: ${target_name}")
    endif()

    target_compile_definitions(
        "${target_name}"
        PRIVATE GAMEEX_VERSION_STRING="${GAMEEX_VERSION}"
    )
endfunction()
