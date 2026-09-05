/**
 * @file desktop_application.hpp
 * @brief Shared composition helper for Game_EX desktop executables.
 */

#pragma once

#include "game_ex/render/renderer.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace game_ex::game {

/**
 * @brief Identifies which user-facing application is being composed.
 * @ingroup game_app
 */
enum class DesktopApplicationRole {
    /** Player-facing game application. */
    game,

    /** Authoring application for inspecting and editing world content. */
    world_editor
};

/**
 * @brief Immutable inputs used to compose a desktop application.
 * @ingroup game_app
 */
struct DesktopApplicationSpecification final {
    /** Functional role used for diagnostics and later composition choices. */
    DesktopApplicationRole role{DesktopApplicationRole::game};

    /** Human-readable application name reported to the operating system. */
    std::string application_name;

    /** Reverse-DNS identifier reported to the operating system. */
    std::string application_identifier;

    /** Text displayed in the initial top-level window title bar. */
    std::string window_title;

    /** Requested initial window width in window coordinates. */
    std::uint32_t window_width{1280};

    /** Requested initial window height in window coordinates. */
    std::uint32_t window_height{720};
};

/**
 * @brief Composes and runs one Game_EX desktop application.
 * @param specification Application identity, role, and initial window details.
 * @param render_frame Owning backend-neutral frame copied into each fresh
 * renderer attempt.
 * @param arguments Process arguments after the executable name.
 * @return Zero after orderly shutdown, or a non-zero value after a reported error.
 *
 * `--renderer=opengl|vulkan|auto` selects an explicit backend or the default
 * ordered automatic policy. `--quit-after-ms=<milliseconds>` exists only for
 * automated window smoke tests. Options may appear in either order; unknown,
 * empty, repeated, and conflicting renderer arguments are rejected.
 *
 * @ingroup game_app
 */
int run_desktop_application(
    const DesktopApplicationSpecification& specification,
    render::RenderFrame render_frame,
    std::span<const std::string_view> arguments);

} // namespace game_ex::game
