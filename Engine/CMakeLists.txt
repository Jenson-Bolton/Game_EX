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
option(GAMEEX_ENABLE_VULKAN "Build the Vulkan 1.3 renderer backend" ON)
set(
    GAMEEX_VULKAN_SDK_ROOT ""
    CACHE PATH
    "Explicit Vulkan SDK root (takes precedence over VULKAN_SDK and conventional Windows installs)"
)

function(gameex_engine_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()
endfunction()

function(gameex_vulkan_sdk_is_valid sdk_root result_variable)
    if(
        EXISTS "${sdk_root}/Include/vulkan/vulkan.h"
        AND EXISTS "${sdk_root}/Lib/vulkan-1.lib"
    )
        set(${result_variable} TRUE PARENT_SCOPE)
    else()
        set(${result_variable} FALSE PARENT_SCOPE)
    endif()
endfunction()

if(GAMEEX_ENABLE_VULKAN)
    set(gameex_selected_vulkan_sdk "")
    if(NOT GAMEEX_VULKAN_SDK_ROOT STREQUAL "")
        gameex_vulkan_sdk_is_valid("${GAMEEX_VULKAN_SDK_ROOT}" gameex_sdk_valid)
        if(NOT gameex_sdk_valid)
            message(FATAL_ERROR
                "GAMEEX_VULKAN_SDK_ROOT is not a valid Vulkan SDK: ${GAMEEX_VULKAN_SDK_ROOT}. "
                "Expected Include/vulkan/vulkan.h and Lib/vulkan-1.lib."
            )
        endif()
        set(gameex_selected_vulkan_sdk "${GAMEEX_VULKAN_SDK_ROOT}")
    elseif(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" gameex_environment_vulkan_sdk)
        gameex_vulkan_sdk_is_valid("${gameex_environment_vulkan_sdk}" gameex_sdk_valid)
        if(NOT gameex_sdk_valid)
            message(FATAL_ERROR
                "VULKAN_SDK is set but does not name a valid Vulkan SDK: "
                "${gameex_environment_vulkan_sdk}. Expected Include/vulkan/vulkan.h "
                "and Lib/vulkan-1.lib."
            )
        endif()
        set(gameex_selected_vulkan_sdk "${gameex_environment_vulkan_sdk}")
    elseif(WIN32)
        file(GLOB gameex_vulkan_sdk_candidates LIST_DIRECTORIES TRUE "C:/VulkanSDK/*")
        list(SORT gameex_vulkan_sdk_candidates COMPARE NATURAL ORDER DESCENDING)
        foreach(gameex_vulkan_sdk_candidate IN LISTS gameex_vulkan_sdk_candidates)
            gameex_vulkan_sdk_is_valid("${gameex_vulkan_sdk_candidate}" gameex_sdk_valid)
            if(gameex_sdk_valid)
                set(gameex_selected_vulkan_sdk "${gameex_vulkan_sdk_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(gameex_selected_vulkan_sdk STREQUAL "")
        message(FATAL_ERROR
            "GAMEEX_ENABLE_VULKAN=ON but no Vulkan SDK was found. Set "
            "GAMEEX_VULKAN_SDK_ROOT, set VULKAN_SDK, install a conventional "
            "Windows SDK under C:/VulkanSDK, or configure with GAMEEX_ENABLE_VULKAN=OFF."
        )
    endif()

    set(Vulkan_INCLUDE_DIR "${gameex_selected_vulkan_sdk}/Include" CACHE PATH "" FORCE)
    set(Vulkan_LIBRARY "${gameex_selected_vulkan_sdk}/Lib/vulkan-1.lib" CACHE FILEPATH "" FORCE)
    find_package(Vulkan 1.3 REQUIRED)
    set(
        GAMEEX_SELECTED_VULKAN_SDK_ROOT "${gameex_selected_vulkan_sdk}"
        CACHE INTERNAL "Vulkan SDK selected by Game_EX"
    )
    set(
        GAMEEX_VULKAN_EMPTY_IMPLICIT_LAYER_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/vulkan-empty-implicit-layers"
        CACHE INTERNAL "Empty loader path used by hermetic Vulkan smoke tests"
    )
    file(MAKE_DIRECTORY "${GAMEEX_VULKAN_EMPTY_IMPLICIT_LAYER_DIRECTORY}")
    message(STATUS "Game_EX Vulkan SDK: ${GAMEEX_SELECTED_VULKAN_SDK_ROOT}")
endif()

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

add_library(
    gameex_render STATIC
    src/render/diagnostic_raster_layout.cpp
    src/render/renderer.cpp
)
add_library(GameEX::Render ALIAS gameex_render)

target_compile_features(gameex_render PUBLIC cxx_std_20)
target_include_directories(
    gameex_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
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

if(GAMEEX_ENABLE_VULKAN)
    add_library(
        gameex_render_vulkan STATIC
        src/render/vulkan/vulkan_policy.cpp
        src/render/vulkan/vulkan_renderer.cpp
    )
    add_library(GameEX::RenderVulkan ALIAS gameex_render_vulkan)

    target_compile_features(gameex_render_vulkan PUBLIC cxx_std_20)
    target_include_directories(
        gameex_render_vulkan
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(
        gameex_render_vulkan
        PUBLIC GameEX::Render
        PRIVATE GameEX::PlatformSDL ${GAMEEX_SDL_TARGET} Vulkan::Vulkan
    )
    gameex_apply_version(gameex_render_vulkan)
    gameex_engine_enable_warnings(gameex_render_vulkan)
endif()

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

    add_executable(
        gameex_diagnostic_raster_layout_tests
        tests/diagnostic_raster_layout_tests.cpp
    )
    target_compile_features(gameex_diagnostic_raster_layout_tests PRIVATE cxx_std_20)
    target_include_directories(
        gameex_diagnostic_raster_layout_tests
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(gameex_diagnostic_raster_layout_tests PRIVATE GameEX::Render)
    gameex_engine_enable_warnings(gameex_diagnostic_raster_layout_tests)

    add_executable(gameex_application_render_tests tests/application_render_tests.cpp)
    target_compile_features(gameex_application_render_tests PRIVATE cxx_std_20)
    target_link_libraries(gameex_application_render_tests PRIVATE GameEX::Core)
    gameex_engine_enable_warnings(gameex_application_render_tests)

    if(GAMEEX_ENABLE_VULKAN)
        add_executable(gameex_vulkan_policy_tests tests/vulkan_policy_tests.cpp)
        target_compile_features(gameex_vulkan_policy_tests PRIVATE cxx_std_20)
        target_include_directories(
            gameex_vulkan_policy_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(
            gameex_vulkan_policy_tests PRIVATE GameEX::RenderVulkan Vulkan::Vulkan
        )
        gameex_engine_enable_warnings(gameex_vulkan_policy_tests)
    endif()

    if(GAMEEX_ENABLE_GUI_SMOKE_TESTS)
        add_executable(gameex_render_opengl_smoke tests/opengl_renderer_smoke.cpp)
        target_compile_features(gameex_render_opengl_smoke PRIVATE cxx_std_20)
        target_link_libraries(
            gameex_render_opengl_smoke
            PRIVATE GameEX::RenderOpenGL GameEX::PlatformSDL
        )
        gameex_apply_version(gameex_render_opengl_smoke)
        gameex_engine_enable_warnings(gameex_render_opengl_smoke)

        if(GAMEEX_ENABLE_VULKAN)
            add_executable(gameex_render_vulkan_smoke tests/vulkan_renderer_smoke.cpp)
            target_compile_features(gameex_render_vulkan_smoke PRIVATE cxx_std_20)
            target_link_libraries(
                gameex_render_vulkan_smoke
                PRIVATE GameEX::RenderVulkan GameEX::PlatformSDL
            )
            gameex_apply_version(gameex_render_vulkan_smoke)
            gameex_engine_enable_warnings(gameex_render_vulkan_smoke)
        endif()
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

    add_test(
        NAME engine.diagnostic_raster_layout
        COMMAND gameex_diagnostic_raster_layout_tests
    )
    set_tests_properties(engine.diagnostic_raster_layout PROPERTIES LABELS "unit")

    add_test(NAME engine.application_render_lifecycle COMMAND gameex_application_render_tests)
    set_tests_properties(engine.application_render_lifecycle PROPERTIES LABELS "unit")

    if(GAMEEX_ENABLE_VULKAN)
        add_test(NAME engine.vulkan_policy COMMAND gameex_vulkan_policy_tests)
        set_tests_properties(engine.vulkan_policy PROPERTIES LABELS "unit;vulkan")
    endif()

    if(GAMEEX_ENABLE_GUI_SMOKE_TESTS)
        add_test(NAME engine.render_opengl_smoke COMMAND gameex_render_opengl_smoke)
        set_tests_properties(
            engine.render_opengl_smoke
            PROPERTIES LABELS "gui;smoke" TIMEOUT 15
        )

        if(GAMEEX_ENABLE_VULKAN)
            add_test(
                NAME engine.render_vulkan_smoke
                COMMAND gameex_render_vulkan_smoke
            )
            set_tests_properties(
                engine.render_vulkan_smoke
                PROPERTIES
                    LABELS "gui;smoke;vulkan"
                    TIMEOUT 15
                    ENVIRONMENT_MODIFICATION
                        "VK_LAYER_PATH=unset:;VK_ADD_LAYER_PATH=unset:;VK_INSTANCE_LAYERS=unset:;VK_LOADER_LAYERS_ENABLE=unset:;VK_LOADER_LAYERS_ALLOW=unset:;VK_IMPLICIT_LAYER_PATH=set:${GAMEEX_VULKAN_EMPTY_IMPLICIT_LAYER_DIRECTORY};VK_ADD_IMPLICIT_LAYER_PATH=unset:;VK_LOADER_DEBUG=unset:;VK_LOADER_LAYERS_DISABLE=set:~implicit~"
            )

            set(gameex_validation_layer_json
                "${GAMEEX_SELECTED_VULKAN_SDK_ROOT}/Bin/VkLayer_khronos_validation.json")
            set(gameex_validation_layer_dll
                "${GAMEEX_SELECTED_VULKAN_SDK_ROOT}/Bin/VkLayer_khronos_validation.dll")
            if(NOT EXISTS "${gameex_validation_layer_json}"
                OR NOT EXISTS "${gameex_validation_layer_dll}")
                message(FATAL_ERROR
                    "Vulkan validation smoke requires VkLayer_khronos_validation.json "
                    "and VkLayer_khronos_validation.dll under the selected SDK Bin directory: "
                    "${GAMEEX_SELECTED_VULKAN_SDK_ROOT}/Bin"
                )
            endif()
            add_test(
                NAME engine.render_vulkan_validation_smoke
                COMMAND gameex_render_vulkan_smoke --validation-required
            )
            set_tests_properties(
                engine.render_vulkan_validation_smoke
                PROPERTIES
                    LABELS "gui;smoke;vulkan;validation"
                    TIMEOUT 15
                    ENVIRONMENT_MODIFICATION
                        "VK_LAYER_PATH=set:${GAMEEX_SELECTED_VULKAN_SDK_ROOT}/Bin;VK_ADD_LAYER_PATH=unset:;VK_INSTANCE_LAYERS=unset:;VK_LOADER_LAYERS_ENABLE=unset:;VK_LOADER_LAYERS_ALLOW=unset:;VK_IMPLICIT_LAYER_PATH=set:${GAMEEX_VULKAN_EMPTY_IMPLICIT_LAYER_DIRECTORY};VK_ADD_IMPLICIT_LAYER_PATH=unset:;VK_LOADER_DEBUG=unset:;VK_LOADER_LAYERS_DISABLE=set:~implicit~"
            )
        endif()
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
