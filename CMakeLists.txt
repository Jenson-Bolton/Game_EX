cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/GameEXVersion.cmake")

project(
    GameEXWorkspace
    VERSION "${GAMEEX_VERSION}"
    DESCRIPTION "Workspace for the Game_EX engine, world compiler, and game"
    LANGUAGES CXX
)

include(CTest)

option(GAMEEX_BUILD_DOCUMENTATION "Create the Doxygen documentation target" ON)
option(GAMEEX_ENABLE_GUI_SMOKE_TESTS "Register tests that briefly open the desktop windows" ON)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

add_subdirectory(Engine)
add_subdirectory(WorldCompiler)
add_subdirectory(Game)

if(GAMEEX_BUILD_DOCUMENTATION)
    find_package(Doxygen 1.9 QUIET)

    if(Doxygen_FOUND)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
            "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            @ONLY
        )

        add_custom_target(
            docs
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen"
            COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Generating Game_EX API documentation"
            VERBATIM
        )
    else()
        message(WARNING "Doxygen was not found; the docs target will not be available.")
    endif()
endif()
