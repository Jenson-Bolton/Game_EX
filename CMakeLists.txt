cmake_minimum_required(VERSION 3.21)

project(Game_EX
  VERSION 0.0.0
  LANGUAGES CXX
)

# --- Language settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------
# Dependencies: SDL3 + GLM
# ----------------------------

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

# ----------------------------
# Source layout (targets)
# ----------------------------

# Helper: glob cpp-like sources
function(glob_cpp out_var base_dir)
  file(GLOB_RECURSE _tmp CONFIGURE_DEPENDS
    "${base_dir}/*.c"
    "${base_dir}/*.cc"
    "${base_dir}/*.cpp"
    "${base_dir}/*.cxx"
  )
  set(${out_var} "${_tmp}" PARENT_SCOPE)
endfunction()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

glob_cpp(ENGINE_SOURCES   "${SRC_DIR}/engine")
glob_cpp(PLATFORM_SOURCES "${SRC_DIR}/platform")
glob_cpp(GAME_SOURCES     "${SRC_DIR}/game")
glob_cpp(APP_SOURCES      "${SRC_DIR}/app")

# Transitional support: if you still have .cpp files directly under src/
# (e.g. src/main.cpp), we’ll compile them into the executable too.
file(GLOB SRC_ROOT_SOURCES CONFIGURE_DEPENDS
  "${SRC_DIR}/*.c"
  "${SRC_DIR}/*.cc"
  "${SRC_DIR}/*.cpp"
  "${SRC_DIR}/*.cxx"
)

# Avoid compiling the same file twice if someone later moves things around weirdly
set(ALL_LIB_SOURCES ${ENGINE_SOURCES} ${PLATFORM_SOURCES} ${GAME_SOURCES})
set(APP_ALL_SOURCES ${APP_SOURCES} ${SRC_ROOT_SOURCES})
foreach(_f IN LISTS ALL_LIB_SOURCES)
  list(REMOVE_ITEM APP_ALL_SOURCES "${_f}")
endforeach()

# ----------------------------
# Libraries
# ----------------------------

add_library(engine STATIC ${ENGINE_SOURCES})
target_include_directories(engine PUBLIC "${SRC_DIR}")
target_link_libraries(engine PUBLIC ${GLM_TARGET})

add_library(platform STATIC ${PLATFORM_SOURCES})
target_include_directories(platform PUBLIC "${SRC_DIR}")
target_link_libraries(platform PUBLIC ${SDL3_TARGET})

add_library(game STATIC ${GAME_SOURCES})
target_include_directories(game PUBLIC "${SRC_DIR}")
target_link_libraries(game PUBLIC engine)

# ----------------------------
# Executable
# ----------------------------

if (APP_ALL_SOURCES STREQUAL "")
  message(FATAL_ERROR
    "No app sources found. Add src/app/main.cpp (recommended) or keep a .cpp in src/ root temporarily."
  )
endif()

add_executable(Game_EX ${APP_ALL_SOURCES})
target_include_directories(Game_EX PRIVATE "${SRC_DIR}")
target_link_libraries(Game_EX PRIVATE engine platform game)

# ----------------------------
# Warnings
# ----------------------------

if (MSVC)
  target_compile_options(engine   PRIVATE /W4)
  target_compile_options(platform PRIVATE /W4)
  target_compile_options(game     PRIVATE /W4)
  target_compile_options(Game_EX  PRIVATE /W4)
else()
  target_compile_options(engine   PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(platform PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(game     PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(Game_EX  PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ----------------------------
# Platform properties
# ----------------------------

if (APPLE)
  set_target_properties(Game_EX PROPERTIES
    MACOSX_BUNDLE TRUE
  )
endif()

# ----------------------------
# Doxygen
# ----------------------------
find_package(Doxygen)

if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()
