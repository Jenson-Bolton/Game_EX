/**
 * @file opengl_renderer.hpp
 * @brief Public composition entry point for the SDL/OpenGL renderer backend.
 */

#pragma once

#include "game_ex/render/renderer_factory.hpp"

#include <memory>

namespace game_ex::render {

/**
 * @brief Creates the OpenGL 4.6 Core renderer factory.
 * @return Uniquely owned factory requiring an OpenGL-capable SDL window.
 * @throws std::bad_alloc if factory allocation fails.
 * @ingroup render_opengl
 */
[[nodiscard]] std::unique_ptr<RendererFactory> create_opengl_renderer_factory();

} // namespace game_ex::render
