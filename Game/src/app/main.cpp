/**
 * @file main.cpp
 * @brief Composition root for the player-facing Game_EX application.
 */

#include "game_ex/game/desktop_application.hpp"

#include "game_ex/render/renderer.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

/**
 * @brief Starts the player-facing game shell.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Process exit status from the shared desktop application lifecycle.
 */
int main(const int argc, char* argv[]) {
    try {
        std::vector<std::string_view> arguments;
        if (argc > 1) {
            arguments.reserve(static_cast<std::size_t>(argc - 1));
        }
        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }

        return game_ex::game::run_desktop_application(
            {
                .role = game_ex::game::DesktopApplicationRole::game,
                .application_name = "Game_EX Game",
                .application_identifier = "dev.game-ex.game",
                .window_title = "Game_EX | Game",
                .window_width = 1280,
                .window_height = 720,
            },
            game_ex::render::foundation_render_frame(),
            arguments);
    } catch (const std::exception& error) {
        std::cerr << "Game_EX game failed during argument preparation: "
                  << error.what() << '\n';
        return 1;
    }
}
