/**
 * @file main.cpp
 * @brief Composition root for the Game_EX world editor.
 */

#include "game_ex/game/desktop_application.hpp"

/**
 * @brief Starts the world-editor shell.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Process exit status from the shared desktop application lifecycle.
 */
int main(const int argc, char* argv[]) {
    return game_ex::game::run_desktop_application(
        {
            .role = game_ex::game::DesktopApplicationRole::world_editor,
            .application_name = "Game_EX World Editor",
            .application_identifier = "dev.game-ex.world-editor",
            .window_title = "Game_EX | World Editor",
            .window_width = 1440,
            .window_height = 900,
        },
        argc,
        argv);
}
