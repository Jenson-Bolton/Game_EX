# Game_EX project version authority.
#
# Every independently configurable subproject includes this file before its
# project() declaration. Increment this value once for each reported release.
include_guard(DIRECTORY)

set(GAMEEX_VERSION "0.1.7")

function(gameex_apply_version target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot apply the Game_EX version to missing target: ${target_name}")
    endif()

    string(REPLACE "." ";" gameex_version_components "${GAMEEX_VERSION}")
    list(GET gameex_version_components 0 gameex_version_major)
    list(GET gameex_version_components 1 gameex_version_minor)
    list(GET gameex_version_components 2 gameex_version_patch)

    target_compile_definitions(
        "${target_name}"
        PRIVATE
            GAMEEX_VERSION_STRING="${GAMEEX_VERSION}"
            GAMEEX_VERSION_MAJOR=${gameex_version_major}
            GAMEEX_VERSION_MINOR=${gameex_version_minor}
            GAMEEX_VERSION_PATCH=${gameex_version_patch}
    )
endfunction()
