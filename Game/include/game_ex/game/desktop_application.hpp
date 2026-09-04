/**
 * @file desktop_application.hpp
 * @brief Shared composition helper for Game_EX desktop executables.
 */

#pragma once

#include <cstdint>
#include <string>

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
 * @param argument_count Number of entries in argument_values.
 * @param argument_values Process command-line argument array.
 * @return Zero after orderly shutdown, or a non-zero value after a reported error.
 *
 * The internal `--quit-after-ms=<milliseconds>` option exists only for automated
 * window smoke tests. Unknown arguments are rejected so accidental configuration
 * errors remain visible during this foundation stage.
 *
 * @ingroup game_app
 */
int run_desktop_application(
    const DesktopApplicationSpecification& specification,
    int argument_count,
    char* argument_values[]);

} // namespace game_ex::game
