/**
 * @file desktop_application.cpp
 * @brief Composition of the game and world-editor desktop shells.
 */

#include "game_ex/game/desktop_application.hpp"

#include "game_ex/core/application.hpp"
#include "game_ex/platform/sdl_platform.hpp"
#include "game_ex/render/opengl_renderer.hpp"
#include "game_ex/world_format/world_header.hpp"
#include "renderer_selection.hpp"

#if GAMEEX_HAS_VULKAN_BACKEND
#include "game_ex/render/vulkan_renderer.hpp"
#endif

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace game_ex::game {
namespace {

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
 * @brief Converts renderer failure categories to stable log text.
 * @param code Failure category to describe.
 * @return Static lowercase category name.
 */
[[nodiscard]] std::string_view error_code_name(
    const render::RendererErrorCode code) noexcept {
    switch (code) {
    case render::RendererErrorCode::invalid_state:
        return "invalid_state";
    case render::RendererErrorCode::incompatible_window:
        return "incompatible_window";
    case render::RendererErrorCode::unavailable:
        return "unavailable";
    case render::RendererErrorCode::initialization_failed:
        return "initialization_failed";
    case render::RendererErrorCode::presentation_failed:
        return "presentation_failed";
    case render::RendererErrorCode::shutdown_failed:
        return "shutdown_failed";
    }
    return "unknown";
}

/**
 * @brief Creates the factory for one explicit renderer attempt.
 * @param backend Backend requested by the selection policy.
 * @return Concrete renderer factory.
 * @throws render::RendererError when Vulkan support was compiled out.
 */
[[nodiscard]] std::unique_ptr<render::RendererFactory> create_renderer_factory(
    const render::RendererBackend backend) {
    if (backend == render::RendererBackend::open_gl) {
        return render::create_opengl_renderer_factory();
    }
#if GAMEEX_HAS_VULKAN_BACKEND
    return render::create_vulkan_renderer_factory();
#else
    throw render::RendererError{
        render::RendererErrorCode::unavailable,
        render::RendererBackend::vulkan,
        "Vulkan renderer support was compiled out; rebuild with GAMEEX_ENABLE_VULKAN=ON"};
#endif
}

/**
 * @brief Captures a renderer error with visibility-phase evidence.
 * @param error Typed renderer error to retain.
 * @param reached_visibility Whether the attempted window was ever shown.
 * @return Failure facts consumed by strict selection policy.
 */
[[nodiscard]] selection_detail::RendererAttemptResult failed_attempt(
    const render::RendererError& error,
    const bool reached_visibility) {
    return {
        .exit_status = 1,
        .failure = selection_detail::RendererAttemptFailure{
            .code = error.code(),
            .backend = error.backend(),
            .stage = reached_visibility
                ? selection_detail::RendererAttemptStage::after_visibility
                : selection_detail::RendererAttemptStage::before_visibility,
            .message = error.what(),
        },
    };
}

/**
 * @brief Runs one backend in a completely fresh platform/application composition.
 * @param specification Shared application identity and window request.
 * @param automatic_exit Optional smoke-test lifetime.
 * @param backend Explicit backend for this one attempt.
 * @return Normal exit status or typed failure facts with visibility phase.
 * @throws Any non-renderer exception without converting it into a fallback signal.
 */
[[nodiscard]] selection_detail::RendererAttemptResult run_renderer_attempt(
    const DesktopApplicationSpecification& specification,
    const std::optional<std::chrono::milliseconds> automatic_exit,
    const render::RendererBackend backend) {
    try {
        std::cout << "Trying renderer: "
                  << render::renderer_backend_name(backend) << '\n';
        auto renderer_factory = create_renderer_factory(backend);
        auto platform = platform::create_sdl_platform({
            .application_name = specification.application_name,
            .application_version = GAMEEX_VERSION_STRING,
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

        core::Application application{
            std::move(platform), window, *renderer_factory, run};
        try {
            const int exit_status = application.run();
            std::cout << "Selected renderer: "
                      << render::renderer_backend_name(backend) << '\n';
            return {.exit_status = exit_status, .failure = std::nullopt};
        } catch (const render::RendererError& error) {
            return failed_attempt(error, application.reached_window_visibility());
        }
    } catch (const render::RendererError& error) {
        return failed_attempt(error, false);
    }
}

} // namespace

int run_desktop_application(
    const DesktopApplicationSpecification& specification,
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
        const selection_detail::DesktopCommandLine command_line =
            selection_detail::parse_command_line(arguments);

        std::cout
            << "Starting Game_EX " << role_name(specification.role)
            << " with world format "
            << world_format::current_major_version << '.'
            << world_format::current_minor_version << '\n';

        return selection_detail::execute_renderer_selection(
            command_line.renderer,
            [&specification, &command_line](const render::RendererBackend backend) {
                return run_renderer_attempt(
                    specification, command_line.automatic_exit_after, backend);
            },
            [](const selection_detail::RendererAttemptFailure& failure) {
                std::cerr << "Automatic Vulkan attempt failed before visibility ["
                          << error_code_name(failure.code) << "]: "
                          << failure.message << "; falling back to opengl\n";
            });
    } catch (const std::exception& error) {
        std::cerr << "Game_EX " << role_name(specification.role)
                  << " failed: " << error.what() << '\n';
        return 1;
    }
}

} // namespace game_ex::game
