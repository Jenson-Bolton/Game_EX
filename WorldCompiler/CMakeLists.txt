cmake_minimum_required(VERSION 3.25)

project(
    GameEXWorldCompiler
    VERSION 0.1.0
    DESCRIPTION "World format and offline compilation tools for Game_EX"
    LANGUAGES CXX
)

include(CTest)

function(gameex_world_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()
endfunction()

add_library(
    gameex_world_format STATIC
    src/world_format/world_header.cpp
)
add_library(GameEX::WorldFormat ALIAS gameex_world_format)

target_compile_features(gameex_world_format PUBLIC cxx_std_20)
target_include_directories(
    gameex_world_format
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
gameex_world_enable_warnings(gameex_world_format)

add_executable(
    game_ex_world_compiler
    tools/world_compiler_main.cpp
)
target_compile_features(game_ex_world_compiler PRIVATE cxx_std_20)
target_link_libraries(game_ex_world_compiler PRIVATE GameEX::WorldFormat)
gameex_world_enable_warnings(game_ex_world_compiler)

if(BUILD_TESTING)
    add_executable(
        gameex_world_format_tests
        tests/world_header_tests.cpp
    )
    target_compile_features(gameex_world_format_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_world_format_tests PRIVATE GameEX::WorldFormat)
    gameex_world_enable_warnings(gameex_world_format_tests)

    add_test(NAME world_format.header COMMAND gameex_world_format_tests)
    set_tests_properties(world_format.header PROPERTIES LABELS "unit")
endif()
