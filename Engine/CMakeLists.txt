cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/GameEXVersion.cmake")

project(
    GameEXEngine
    VERSION "${GAMEEX_VERSION}"
    DESCRIPTION "Reusable engine foundation for Game_EX"
    LANGUAGES CXX
)

include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

option(GAMEEX_ENGINE_USE_SYSTEM_SDL3 "Use an installed SDL3 package instead of the pinned source release" OFF)

function(gameex_engine_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()
endfunction()

if(GAMEEX_ENGINE_USE_SYSTEM_SDL3)
    find_package(SDL3 3.4 CONFIG REQUIRED)
    set(GAMEEX_SDL_TARGET SDL3::SDL3)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        URL "https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz"
        URL_HASH "SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3)
    set(GAMEEX_SDL_TARGET SDL3::SDL3-static)
endif()

add_library(gameex_jobs STATIC src/jobs/job_system.cpp)
add_library(GameEX::Jobs ALIAS gameex_jobs)

target_compile_features(gameex_jobs PUBLIC cxx_std_20)
target_include_directories(
    gameex_jobs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gameex_jobs PUBLIC Threads::Threads)
gameex_engine_enable_warnings(gameex_jobs)

add_library(gameex_startup STATIC src/startup/startup_graph.cpp)
add_library(GameEX::Startup ALIAS gameex_startup)

target_compile_features(gameex_startup PUBLIC cxx_std_20)
target_include_directories(
    gameex_startup
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gameex_startup PUBLIC GameEX::Jobs)
gameex_engine_enable_warnings(gameex_startup)

add_library(gameex_core STATIC src/core/application.cpp)
add_library(GameEX::Core ALIAS gameex_core)

target_compile_features(gameex_core PUBLIC cxx_std_20)
target_include_directories(
    gameex_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gameex_core PUBLIC GameEX::Startup)
gameex_engine_enable_warnings(gameex_core)

add_library(
    gameex_platform_sdl STATIC
    src/platform/sdl/sdl_platform.cpp
)
add_library(GameEX::PlatformSDL ALIAS gameex_platform_sdl)

target_compile_features(gameex_platform_sdl PUBLIC cxx_std_20)
target_include_directories(
    gameex_platform_sdl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gameex_platform_sdl PRIVATE ${GAMEEX_SDL_TARGET})
gameex_engine_enable_warnings(gameex_platform_sdl)

if(BUILD_TESTING)
    add_executable(gameex_jobs_tests tests/job_system_tests.cpp)
    target_compile_features(gameex_jobs_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_jobs_tests PRIVATE GameEX::Jobs)
    gameex_engine_enable_warnings(gameex_jobs_tests)

    add_executable(gameex_startup_tests tests/startup_graph_tests.cpp)
    target_compile_features(gameex_startup_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_startup_tests PRIVATE GameEX::Startup)
    gameex_engine_enable_warnings(gameex_startup_tests)

    add_test(NAME engine.job_system COMMAND gameex_jobs_tests)
    set_tests_properties(engine.job_system PROPERTIES LABELS "unit")

    add_test(
        NAME engine.job_system_wrong_thread_destruction
        COMMAND gameex_jobs_tests --death-destroy-outside-creator
    )
    set_tests_properties(
        engine.job_system_wrong_thread_destruction
        PROPERTIES LABELS "unit;death" TIMEOUT 10 WILL_FAIL TRUE
    )

    add_test(NAME engine.startup_graph COMMAND gameex_startup_tests)
    set_tests_properties(engine.startup_graph PROPERTIES LABELS "unit")

    add_test(
        NAME engine.startup_graph_wrong_thread_destruction
        COMMAND gameex_startup_tests --death-destroy-outside-creator
    )
    set_tests_properties(
        engine.startup_graph_wrong_thread_destruction
        PROPERTIES LABELS "unit;death" TIMEOUT 10 WILL_FAIL TRUE
    )
endif()
