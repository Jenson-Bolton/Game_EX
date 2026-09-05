cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/GameEXVersion.cmake")

project(
    GameEXGame
    VERSION "${GAMEEX_VERSION}"
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
    src/common/renderer_selection.cpp
)
add_library(GameEX::GameApp ALIAS gameex_game_app)

target_compile_features(gameex_game_app PUBLIC cxx_std_20)
target_include_directories(
    gameex_game_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
)
target_link_libraries(
    gameex_game_app
    PRIVATE
        GameEX::Core
        GameEX::PlatformSDL
        GameEX::RenderOpenGL
        GameEX::WorldFormat
)
if(TARGET GameEX::RenderVulkan)
    target_link_libraries(gameex_game_app PRIVATE GameEX::RenderVulkan)
    target_compile_definitions(gameex_game_app PRIVATE GAMEEX_HAS_VULKAN_BACKEND=1)
else()
    target_compile_definitions(gameex_game_app PRIVATE GAMEEX_HAS_VULKAN_BACKEND=0)
endif()
gameex_apply_version(gameex_game_app)
gameex_game_enable_warnings(gameex_game_app)

add_executable(game_ex src/app/main.cpp)
target_compile_features(game_ex PRIVATE cxx_std_20)
target_link_libraries(game_ex PRIVATE GameEX::GameApp)
gameex_game_enable_warnings(game_ex)

add_executable(game_ex_world_editor src/editor/main.cpp)
target_compile_features(game_ex_world_editor PRIVATE cxx_std_20)
target_link_libraries(game_ex_world_editor PRIVATE GameEX::GameApp)
gameex_game_enable_warnings(game_ex_world_editor)

if(BUILD_TESTING)
    add_executable(
        gameex_renderer_selection_tests
        tests/renderer_selection_tests.cpp
        src/common/renderer_selection.cpp
    )
    target_compile_features(gameex_renderer_selection_tests PRIVATE cxx_std_20)
    target_include_directories(
        gameex_renderer_selection_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
    )
    target_link_libraries(gameex_renderer_selection_tests PRIVATE GameEX::Render)
    gameex_game_enable_warnings(gameex_renderer_selection_tests)
    add_test(NAME game.renderer_selection COMMAND gameex_renderer_selection_tests)
    set_tests_properties(game.renderer_selection PROPERTIES LABELS "unit")
endif()

if(BUILD_TESTING AND GAMEEX_ENABLE_GUI_SMOKE_TESTS)
    add_test(
        NAME game.window_opengl_smoke
        COMMAND game_ex --renderer=opengl --quit-after-ms=250
    )
    set_tests_properties(game.window_opengl_smoke PROPERTIES LABELS "gui;smoke;opengl" TIMEOUT 15)

    add_test(
        NAME world_editor.window_opengl_smoke
        COMMAND game_ex_world_editor --quit-after-ms=250 --renderer=opengl
    )
    set_tests_properties(world_editor.window_opengl_smoke PROPERTIES LABELS "gui;smoke;opengl" TIMEOUT 15)

    if(TARGET GameEX::RenderVulkan)
        add_test(
            NAME game.window_vulkan_smoke
            COMMAND game_ex --renderer=vulkan --quit-after-ms=250
        )
        add_test(
            NAME world_editor.window_vulkan_smoke
            COMMAND game_ex_world_editor --quit-after-ms=250 --renderer=vulkan
        )
        add_test(
            NAME game.window_auto_smoke
            COMMAND game_ex --quit-after-ms=250
        )
        foreach(gameex_vulkan_app_test IN ITEMS
            game.window_vulkan_smoke
            world_editor.window_vulkan_smoke
            game.window_auto_smoke
        )
            set_tests_properties(
                ${gameex_vulkan_app_test}
                PROPERTIES
                    LABELS "gui;smoke;vulkan"
                    TIMEOUT 15
                    ENVIRONMENT_MODIFICATION
                        "VK_LAYER_PATH=unset:;VK_ADD_LAYER_PATH=unset:;VK_INSTANCE_LAYERS=unset:;VK_LOADER_LAYERS_ENABLE=unset:;VK_LOADER_LAYERS_ALLOW=unset:;VK_IMPLICIT_LAYER_PATH=set:${GAMEEX_VULKAN_EMPTY_IMPLICIT_LAYER_DIRECTORY};VK_ADD_IMPLICIT_LAYER_PATH=unset:;VK_LOADER_DEBUG=unset:;VK_LOADER_LAYERS_DISABLE=set:~implicit~"
            )
        endforeach()
    endif()
endif()
