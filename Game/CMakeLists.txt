cmake_minimum_required(VERSION 3.25)

project(
    GameEXGame
    VERSION 0.1.0
    DESCRIPTION "Game_EX game and world-editor applications"
    LANGUAGES CXX
)

include(CTest)

option(GAMEEX_ENABLE_GUI_SMOKE_TESTS "Register tests that briefly open the desktop windows" ON)

function(gameex_game_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()
endfunction()

# A standalone configure of Game can compose the sibling projects in this local
# workspace. Installed-package discovery will replace this bridge when the three
# projects become separate repositories.
if(NOT TARGET GameEX::Core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../Engine"
        "${CMAKE_CURRENT_BINARY_DIR}/dependencies/Engine"
    )
endif()

if(NOT TARGET GameEX::WorldFormat)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../WorldCompiler"
        "${CMAKE_CURRENT_BINARY_DIR}/dependencies/WorldCompiler"
    )
endif()

add_library(
    gameex_game_app STATIC
    src/common/desktop_application.cpp
)
add_library(GameEX::GameApp ALIAS gameex_game_app)

target_compile_features(gameex_game_app PUBLIC cxx_std_20)
target_include_directories(
    gameex_game_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    gameex_game_app
    PRIVATE
        GameEX::Core
        GameEX::PlatformSDL
        GameEX::WorldFormat
)
gameex_game_enable_warnings(gameex_game_app)

add_executable(game_ex src/app/main.cpp)
target_compile_features(game_ex PRIVATE cxx_std_20)
target_link_libraries(game_ex PRIVATE GameEX::GameApp)
gameex_game_enable_warnings(game_ex)

add_executable(game_ex_world_editor src/editor/main.cpp)
target_compile_features(game_ex_world_editor PRIVATE cxx_std_20)
target_link_libraries(game_ex_world_editor PRIVATE GameEX::GameApp)
gameex_game_enable_warnings(game_ex_world_editor)

if(BUILD_TESTING AND GAMEEX_ENABLE_GUI_SMOKE_TESTS)
    add_test(NAME game.window_smoke COMMAND game_ex --quit-after-ms=250)
    set_tests_properties(game.window_smoke PROPERTIES LABELS "gui;smoke" TIMEOUT 15)

    add_test(NAME world_editor.window_smoke COMMAND game_ex_world_editor --quit-after-ms=250)
    set_tests_properties(world_editor.window_smoke PROPERTIES LABELS "gui;smoke" TIMEOUT 15)
endif()
