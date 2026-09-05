/**
 * @file sdl_window_access.hpp
 * @brief Private checked bridge from platform-neutral windows to SDL.
 */

#pragma once

#include "game_ex/platform/window.hpp"

struct SDL_Window;

namespace game_ex::platform::sdl_detail {

/**
 * @brief Obtains a native SDL window only when type and graphics flags match.
 * @param window Platform-neutral window to inspect without taking ownership.
 * @param required_api Graphics capability required by the renderer backend.
 * @return Native SDL handle, or null for a foreign/incompatible window.
 */
[[nodiscard]] SDL_Window* native_window(
    Window& window,
    WindowGraphicsApi required_api) noexcept;

} // namespace game_ex::platform::sdl_detail
