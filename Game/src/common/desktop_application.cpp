/**
 * @file desktop_application.cpp
 * @brief Composition of the game and world-editor desktop shells.
 */

#include "game_ex/game/desktop_application.hpp"

#include "game_ex/core/application.hpp"
#include "game_ex/platform/sdl_platform.hpp"
#include "game_ex/world_format/world_header.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace game_ex::game {
namespace {

/** Prefix for the automation-only window lifetime option. */
constexpr std::string_view quit_after_prefix{"--quit-after-ms="};

/**
 * @brief Converts an application role into stable diagnostic text.
 * @param role Role supplied by an executable composition root.
 * @return Non-owning static role name.
 */
[[nodiscard]] std::string_view role_name(const DesktopApplicationRole role) noexcept {
    switch (role) {
    case DesktopApplicationRole::game:
        return "game";
    case DesktopApplicationRole::world_editor:
        return "world editor";
    }

    return "unknown";
}

/**
 * @brief Parses the optional automatic-exit duration used by GUI smoke tests.
 * @param argument_count Number of command-line array entries.
 * @param argument_values Process command-line array.
 * @return Empty for a normal interactive run, otherwise the requested duration.
 * @throws std::invalid_argument if an argument is unknown or malformed.
 */
[[nodiscard]] std::optional<std::chrono::milliseconds> parse_automatic_exit(
    const int argument_count,
    char* argument_values[]) {
    std::optional<std::chrono::milliseconds> result;

    for (int index = 1; index < argument_count; ++index) {
        const std::string_view argument{argument_values[index]};
        if (!argument.starts_with(quit_after_prefix)) {
            throw std::invalid_argument("Unknown command-line argument: " + std::string{argument});
        }

        if (result.has_value()) {
            throw std::invalid_argument("--quit-after-ms may be supplied only once");
        }

        const std::string_view value_text = argument.substr(quit_after_prefix.size());
        std::uint64_t milliseconds{};
        const auto parse_result = std::from_chars(
            value_text.data(), value_text.data() + value_text.size(), milliseconds);
        if (parse_result.ec != std::errc{}
            || parse_result.ptr != value_text.data() + value_text.size()) {
            throw std::invalid_argument("--quit-after-ms requires an unsigned integer");
        }

        constexpr std::uint64_t maximum_duration = 86'400'000;
        if (milliseconds > maximum_duration) {
            throw std::invalid_argument("--quit-after-ms cannot exceed 24 hours");
        }

        result = std::chrono::milliseconds{milliseconds};
    }

    return result;
}

} // namespace

int run_desktop_application(
    const DesktopApplicationSpecification& specification,
    const int argument_count,
    char* argument_values[]) {
    try {
        const auto automatic_exit = parse_automatic_exit(argument_count, argument_values);

        std::cout
            << "Starting Game_EX " << role_name(specification.role)
            << " with world format "
            << world_format::current_major_version << '.'
            << world_format::current_minor_version << '\n';

        auto platform = platform::create_sdl_platform({
            .application_name = specification.application_name,
            .application_version = "0.1.0",
            .application_identifier = specification.application_identifier,
        });

        const platform::WindowSpecification window{
            .title = specification.window_title,
            .width = specification.window_width,
            .height = specification.window_height,
            .resizable = true,
        };

        const core::RunConfiguration run{
            .automatic_exit_after = automatic_exit,
            .idle_sleep = std::chrono::milliseconds{8},
        };

        core::Application application{std::move(platform), window, run};
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "Game_EX " << role_name(specification.role)
                  << " failed: " << error.what() << '\n';
        return 1;
    }
}

} // namespace game_ex::game
