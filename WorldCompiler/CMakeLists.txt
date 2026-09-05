cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/GameEXVersion.cmake")

project(
    GameEXWorldCompiler
    VERSION "${GAMEEX_VERSION}"
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
    src/world_format/world_package.cpp
    src/world_format/package_validation.cpp
)
add_library(GameEX::WorldFormat ALIAS gameex_world_format)

target_compile_features(gameex_world_format PUBLIC cxx_std_20)
target_include_directories(
    gameex_world_format
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
gameex_world_enable_warnings(gameex_world_format)

add_library(
    gameex_world_compiler_core STATIC
    src/compiler/world_compiler.cpp
    src/compiler/package_encoder.cpp
)
add_library(GameEX::WorldCompilerCore ALIAS gameex_world_compiler_core)

target_compile_features(gameex_world_compiler_core PUBLIC cxx_std_20)
target_include_directories(
    gameex_world_compiler_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/compiler"
)
target_link_libraries(gameex_world_compiler_core PUBLIC GameEX::WorldFormat)
gameex_world_enable_warnings(gameex_world_compiler_core)

add_executable(
    game_ex_world_compiler
    tools/world_compiler_main.cpp
)
target_compile_features(game_ex_world_compiler PRIVATE cxx_std_20)
target_link_libraries(game_ex_world_compiler PRIVATE GameEX::WorldCompilerCore)
gameex_apply_version(game_ex_world_compiler)
gameex_world_enable_warnings(game_ex_world_compiler)

if(BUILD_TESTING)
    find_package(Threads REQUIRED)

    add_executable(
        gameex_world_format_tests
        tests/world_header_tests.cpp
    )
    target_compile_features(gameex_world_format_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_world_format_tests PRIVATE GameEX::WorldFormat)
    gameex_world_enable_warnings(gameex_world_format_tests)

    add_test(NAME world_format.header COMMAND gameex_world_format_tests)
    set_tests_properties(world_format.header PROPERTIES LABELS "unit")

    add_executable(
        gameex_world_format_runtime_tests
        tests/world_format_runtime_tests.cpp
    )
    target_compile_features(gameex_world_format_runtime_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        gameex_world_format_runtime_tests
        PRIVATE GAMEEX_WORLD_GOLDEN_HEX="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/minimal/expected.gexworld.hex"
    )
    target_link_libraries(gameex_world_format_runtime_tests PRIVATE GameEX::WorldFormat)
    gameex_world_enable_warnings(gameex_world_format_runtime_tests)

    add_test(NAME world_format.runtime_fixture COMMAND gameex_world_format_runtime_tests)
    set_tests_properties(world_format.runtime_fixture PROPERTIES LABELS "unit;integration")

    add_executable(
        gameex_world_compiler_tests
        tests/world_compiler_tests.cpp
    )
    target_compile_features(gameex_world_compiler_tests PRIVATE cxx_std_20)
    target_include_directories(
        gameex_world_compiler_tests
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_definitions(
        gameex_world_compiler_tests
        PRIVATE GAMEEX_WORLD_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/minimal"
    )
    target_link_libraries(
        gameex_world_compiler_tests
        PRIVATE GameEX::WorldCompilerCore Threads::Threads
    )
    gameex_world_enable_warnings(gameex_world_compiler_tests)

    add_test(NAME world_compiler.pipeline COMMAND gameex_world_compiler_tests)
    set_tests_properties(world_compiler.pipeline PROPERTIES LABELS "unit;integration")

    add_test(
        NAME world_compiler.cli
        COMMAND
            "${CMAKE_COMMAND}"
            "-DWORLD_COMPILER=$<TARGET_FILE:game_ex_world_compiler>"
            "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cli_test"
            "-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/minimal"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/world_compiler_cli_tests.cmake"
    )
    set_tests_properties(world_compiler.cli PROPERTIES LABELS "integration;cli")
endif()
