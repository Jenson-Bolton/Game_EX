cmake_minimum_required(VERSION 3.25)

project(
    GameEXEngine
    VERSION 0.1.0
    DESCRIPTION "Reusable engine foundation for Game_EX"
    LANGUAGES CXX
)

include(FetchContent)

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

add_library(
    gameex_core STATIC
    src/core/application.cpp
)
add_library(GameEX::Core ALIAS gameex_core)

target_compile_features(gameex_core PUBLIC cxx_std_20)
target_include_directories(
    gameex_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
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
