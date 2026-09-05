cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/GameEXVersion.cmake")

project(
    GameEXEngine
    VERSION "${GAMEEX_VERSION}"
    DESCRIPTION "Reusable engine foundation for Game_EX"
    LANGUAGES C CXX
)

include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

option(GAMEEX_ENGINE_USE_SYSTEM_SDL3 "Use an installed SDL3 package instead of the pinned source release" OFF)
option(GAMEEX_ENABLE_GUI_SMOKE_TESTS "Register tests that briefly open desktop windows" ON)

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

add_library(gameex_platform INTERFACE)
add_library(GameEX::Platform ALIAS gameex_platform)

target_compile_features(gameex_platform INTERFACE cxx_std_20)
target_include_directories(
    gameex_platform
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_library(gameex_render STATIC src/render/renderer.cpp)
add_library(GameEX::Render ALIAS gameex_render)

target_compile_features(gameex_render PUBLIC cxx_std_20)
target_include_directories(
    gameex_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gameex_render PUBLIC GameEX::Platform)
gameex_engine_enable_warnings(gameex_render)

add_library(gameex_core STATIC src/core/application.cpp)
add_library(GameEX::Core ALIAS gameex_core)

target_compile_features(gameex_core PUBLIC cxx_std_20)
target_include_directories(
    gameex_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gameex_core PUBLIC GameEX::Startup GameEX::Render)
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
target_link_libraries(gameex_platform_sdl PUBLIC GameEX::Platform)
target_link_libraries(gameex_platform_sdl PRIVATE ${GAMEEX_SDL_TARGET})
gameex_engine_enable_warnings(gameex_platform_sdl)

set(GAMEEX_GLAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(GAMEEX_GLAD_GL_HEADER "${GAMEEX_GLAD_ROOT}/include/glad/gl.h")
set(GAMEEX_GLAD_KHR_HEADER "${GAMEEX_GLAD_ROOT}/include/KHR/khrplatform.h")
set(GAMEEX_GLAD_SOURCE "${GAMEEX_GLAD_ROOT}/src/gl.c")

set(GAMEEX_GLAD_GL_HEADER_SHA256 "0E1AC72E82AEC8AB8D1FE176E46A471643C9673B03BA42C4DF6DEA89D2F0E7F5")
set(GAMEEX_GLAD_KHR_HEADER_SHA256 "7B1E01AAA7AD8F6FC34B5C7BDF79EBF5189BB09E2C4D2E79FC5D350623D11E83")
set(GAMEEX_GLAD_SOURCE_SHA256 "07CF9E52E7C018A1E5A8FDE75BFE63F0527B3963B9F5F6219AE04924FBE934C1")

function(gameex_verify_vendored_file file_path expected_hash)
    file(SHA256 "${file_path}" actual_hash)
    string(TOUPPER "${actual_hash}" actual_hash_upper)
    if(NOT actual_hash_upper STREQUAL "${expected_hash}")
        message(FATAL_ERROR "Vendored GLAD file hash mismatch: ${file_path}")
    endif()
endfunction()

gameex_verify_vendored_file("${GAMEEX_GLAD_GL_HEADER}" "${GAMEEX_GLAD_GL_HEADER_SHA256}")
gameex_verify_vendored_file("${GAMEEX_GLAD_KHR_HEADER}" "${GAMEEX_GLAD_KHR_HEADER_SHA256}")
gameex_verify_vendored_file("${GAMEEX_GLAD_SOURCE}" "${GAMEEX_GLAD_SOURCE_SHA256}")

add_library(gameex_glad STATIC "${GAMEEX_GLAD_SOURCE}")
target_compile_features(gameex_glad PUBLIC c_std_99)
target_include_directories(gameex_glad SYSTEM PUBLIC "${GAMEEX_GLAD_ROOT}/include")
set_target_properties(gameex_glad PROPERTIES FOLDER "ThirdParty")

add_library(
    gameex_render_opengl STATIC
    src/render/opengl/opengl_renderer.cpp
)
add_library(GameEX::RenderOpenGL ALIAS gameex_render_opengl)

target_compile_features(gameex_render_opengl PUBLIC cxx_std_20)
target_include_directories(
    gameex_render_opengl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(
    gameex_render_opengl
    PUBLIC GameEX::Render
    PRIVATE GameEX::PlatformSDL ${GAMEEX_SDL_TARGET} gameex_glad
)
gameex_engine_enable_warnings(gameex_render_opengl)

if(BUILD_TESTING)
    add_executable(gameex_jobs_tests tests/job_system_tests.cpp)
    target_compile_features(gameex_jobs_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_jobs_tests PRIVATE GameEX::Jobs)
    gameex_engine_enable_warnings(gameex_jobs_tests)

    add_executable(gameex_startup_tests tests/startup_graph_tests.cpp)
    target_compile_features(gameex_startup_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_startup_tests PRIVATE GameEX::Startup)
    gameex_engine_enable_warnings(gameex_startup_tests)

    add_executable(gameex_render_tests tests/render_tests.cpp)
    target_compile_features(gameex_render_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_render_tests PRIVATE GameEX::Render)
    gameex_engine_enable_warnings(gameex_render_tests)

    add_executable(gameex_application_render_tests tests/application_render_tests.cpp)
    target_compile_features(gameex_application_render_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_application_render_tests PRIVATE GameEX::Core)
    gameex_engine_enable_warnings(gameex_application_render_tests)

    if(GAMEEX_ENABLE_GUI_SMOKE_TESTS)
        add_executable(gameex_render_opengl_smoke tests/opengl_renderer_smoke.cpp)
        target_compile_features(gameex_render_opengl_smoke PRIVATE cxx_std_20)
        target_link_libraries(
            gameex_render_opengl_smoke
            PRIVATE GameEX::RenderOpenGL GameEX::PlatformSDL
        )
        gameex_apply_version(gameex_render_opengl_smoke)
        gameex_engine_enable_warnings(gameex_render_opengl_smoke)
    endif()

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

    add_test(NAME engine.render_api COMMAND gameex_render_tests)
    set_tests_properties(engine.render_api PROPERTIES LABELS "unit")

    add_test(NAME engine.application_render_lifecycle COMMAND gameex_application_render_tests)
    set_tests_properties(engine.application_render_lifecycle PROPERTIES LABELS "unit")

    if(GAMEEX_ENABLE_GUI_SMOKE_TESTS)
        add_test(NAME engine.render_opengl_smoke COMMAND gameex_render_opengl_smoke)
        set_tests_properties(
            engine.render_opengl_smoke
            PROPERTIES LABELS "gui;smoke" TIMEOUT 15
        )
    endif()

    add_test(
        NAME engine.startup_graph_wrong_thread_destruction
        COMMAND gameex_startup_tests --death-destroy-outside-creator
    )
    set_tests_properties(
        engine.startup_graph_wrong_thread_destruction
        PROPERTIES LABELS "unit;death" TIMEOUT 10 WILL_FAIL TRUE
    )
endif()
