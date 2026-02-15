cmake_minimum_required(VERSION 3.21)

project(Game_EX
  VERSION 0.0.0
  LANGUAGES CXX
)

# --- Language settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Collect all source files under src/ (including subfolders) ---
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hh"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hxx"
)

if (GAME_SOURCES STREQUAL "")
  message(FATAL_ERROR "No source files found under src/. Add at least one .cpp file.")
endif()

add_executable(Game_EX
  ${GAME_SOURCES}
  ${GAME_HEADERS}
)

target_include_directories(Game_EX PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# --- SDL3 dependency ---
if (EMSCRIPTEN)
  # Build SDL3 from source for web
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON  CACHE BOOL "" FORCE)
  set(SDL_TEST OFF   CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

  add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

  set(SDL3_TARGET SDL3::SDL3-static)
else()
  # Native platforms: use vcpkg-provided SDL3
  find_package(SDL3 CONFIG REQUIRED)
  set(SDL3_TARGET SDL3::SDL3)
endif()

# --- GLM dependency ---
if (EMSCRIPTEN)
  add_library(glm_headers INTERFACE)
  target_include_directories(glm_headers INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/glm
  )
  set(GLM_TARGET glm_headers)
else()
  find_package(glm CONFIG REQUIRED)
  set(GLM_TARGET glm::glm)
endif()

target_link_libraries(Game_EX PRIVATE ${SDL3_TARGET} ${GLM_TARGET})

# --- Warnings ---
if (MSVC)
  target_compile_options(Game_EX PRIVATE /W4)
else()
  target_compile_options(Game_EX PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Platform ---
if (APPLE)
  set_target_properties(Game_EX PROPERTIES
    MACOSX_BUNDLE TRUE
  )
endif()
