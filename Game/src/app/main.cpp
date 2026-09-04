/**
 * @file main.cpp
 * @brief Composition root for the player-facing Game_EX application.
 */

#include "game_ex/game/desktop_application.hpp"

/**
 * @brief Starts the player-facing game shell.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Process exit status from the shared desktop application lifecycle.
 */
int main(const int argc, char* argv[]) {
    return game_ex::game::run_desktop_application(
        {
            .role = game_ex::game::DesktopApplicationRole::game,
            .application_name = "Game_EX Game",
            .application_identifier = "dev.game-ex.game",
            .window_title = "Game_EX | Game",
            .window_width = 1280,
            .window_height = 720,
        },
        argc,
        argv);
}
