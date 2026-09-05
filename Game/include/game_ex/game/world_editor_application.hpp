/**
 * @file world_editor_application.hpp
 * @brief Composition boundary for the standalone Game_EX world editor.
 */

#pragma once

namespace game_ex::game {

/**
 * @brief Loads optional world data and runs the standalone world editor.
 * @param argument_count Number of process command-line arguments.
 * @param argument_values Process command-line argument array.
 * @return Zero after orderly shutdown, or a non-zero value after a reported error.
 *
 * The editor-specific `--world=<path>` option is parsed and removed before the
 * remaining renderer and smoke-test options reach the shared desktop runner.
 * When a world path is present, its complete package is read, integrity checked,
 * decoded, and converted to an owning diagnostic raster before any platform,
 * window, or renderer attempt begins.
 *
 * @ingroup game_app
 */
int run_world_editor_application(int argument_count, char* argument_values[]);

} // namespace game_ex::game
