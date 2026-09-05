/**
 * @file main.cpp
 * @brief Composition root for the Game_EX world editor.
 */

#include "game_ex/game/world_editor_application.hpp"

/**
 * @brief Starts the world-editor shell.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Process exit status from the shared desktop application lifecycle.
 */
int main(const int argc, char* argv[]) {
    return game_ex::game::run_world_editor_application(argc, argv);
}
