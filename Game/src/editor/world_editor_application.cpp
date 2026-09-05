/**
 * @file world_editor_application.cpp
 * @brief Pre-window package loading and standalone editor composition.
 */

#include "game_ex/game/world_editor_application.hpp"

#include "game_ex/game/desktop_application.hpp"
#include "game_ex/render/renderer.hpp"
#include "game_ex/world_format/world_header.hpp"
#include "game_ex/world_format/world_package.hpp"
#include "world_editor_model.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace game_ex::game {

int run_world_editor_application(
    const int argument_count,
    char* argument_values[]) {
    try {
        std::vector<std::string_view> arguments;
        if (argument_count > 1) {
            arguments.reserve(static_cast<std::size_t>(argument_count - 1));
        }
        for (int index = 1; index < argument_count; ++index) {
            arguments.emplace_back(argument_values[index]);
        }

        world_editor_detail::WorldEditorCommandLine command_line =
            world_editor_detail::parse_world_editor_command_line(arguments);
        render::RenderFrame render_frame = render::foundation_render_frame();

        if (command_line.world_path.has_value()) {
            const world_format::WorldPackage package =
                world_format::read_world_package(command_line.world_path.value());
            world_editor_detail::TerrainVisualization visualization =
                world_editor_detail::build_terrain_visualization(package);
            world_editor_detail::write_world_visualization_summary(
                std::cout,
                command_line.world_path.value(),
                package,
                visualization);
            render_frame = std::move(visualization.render_frame);
        } else {
            std::cout << "World package: none; displaying foundation shell with world format "
                      << world_format::current_major_version << '.'
                      << world_format::current_minor_version << '\n';
        }

        return run_desktop_application(
            {
                .role = DesktopApplicationRole::world_editor,
                .application_name = "Game_EX World Editor",
                .application_identifier = "dev.game-ex.world-editor",
                .window_title = "Game_EX | World Editor",
                .window_width = 1440,
                .window_height = 900,
            },
            std::move(render_frame),
            command_line.desktop_arguments);
    } catch (const std::exception& error) {
        std::cerr << "Game_EX world editor failed before desktop composition: "
                  << error.what() << '\n';
        return 1;
    }
}

} // namespace game_ex::game
