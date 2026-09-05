/**
 * @file vulkan_renderer_smoke.cpp
 * @brief Real SDL/Vulkan 1.3 diagnostic-raster presentation smoke test.
 */

#include "game_ex/platform/sdl_platform.hpp"
#include "game_ex/render/vulkan_renderer.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

/**
 * @brief Reports one failed assertion without skipping renderer cleanup.
 * @param condition Assertion result.
 * @param description Human-readable assertion description.
 * @return The unchanged assertion result.
 */
bool check(const bool condition, const std::string_view description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
    }
    return condition;
}

/**
 * @brief Parses the smoke-test-only validation switch.
 * @param argument_count Number of command-line array entries.
 * @param argument_values Process command-line array.
 * @return Required validation mode when explicitly requested, otherwise
 * disabled.
 * @throws std::invalid_argument for any unsupported argument shape.
 */
game_ex::render::VulkanValidationMode validation_mode(const int argument_count,
                                                      char* argument_values[]) {
    if (argument_count == 1) {
        return game_ex::render::VulkanValidationMode::disabled;
    }
    if (argument_count == 2 && std::string_view{argument_values[1]} == "--validation-required") {
        return game_ex::render::VulkanValidationMode::required;
    }
    throw std::invalid_argument("Usage: gameex_render_vulkan_smoke [--validation-required]");
}

/**
 * @brief Runs a real 2x2 raster until repeated native presentation succeeds.
 * @param mode Validation-layer availability policy.
 * @return True when API, presentation, and validation evidence meet the
 * baseline.
 */
bool run_smoke(const game_ex::render::VulkanValidationMode mode) {
    const game_ex::render::RenderFrame frame{
        .background = game_ex::render::foundation_diagnostic_frame(),
        .raster = game_ex::render::DiagnosticRaster{
            .columns = 2U,
            .rows = 2U,
            .display_aspect_ratio = 1.0F,
            .linear_colours = {
                {.red = 0.85F, .green = 0.10F, .blue = 0.10F, .alpha = 1.0F},
                {.red = 0.10F, .green = 0.85F, .blue = 0.10F, .alpha = 1.0F},
                {.red = 0.10F, .green = 0.10F, .blue = 0.85F, .alpha = 1.0F},
                {.red = 0.85F, .green = 0.85F, .blue = 0.10F, .alpha = 1.0F},
            },
        },
    };
    auto platform = game_ex::platform::create_sdl_platform({
        .application_name = "Game_EX Vulkan smoke",
        .application_version = GAMEEX_VERSION_STRING,
        .application_identifier = "dev.game-ex.tests.vulkan-smoke",
    });
    auto factory = game_ex::render::create_vulkan_renderer_factory({.validation = mode});
    game_ex::platform::WindowSpecification specification{
        .title = "Game_EX Vulkan 1.3 diagnostic smoke",
        .width = 320U,
        .height = 200U,
        .resizable = false,
        .graphics_api = factory->required_window_api(),
    };
    auto window = platform->create_window(specification);
    auto renderer = factory->create(*window);

    renderer->start();
    static_cast<void>(renderer->render_frame(frame));
    window->show();

    constexpr std::uint32_t required_visible_presentations{32U};
    std::uint32_t visible_presentations{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (visible_presentations < required_visible_presentations
           && std::chrono::steady_clock::now() < deadline) {
        if (platform->pump_events() == game_ex::platform::EventPumpResult::exit_requested) {
            break;
        }
        const auto result = renderer->render_frame(frame);
        if (result == game_ex::render::FramePresentationResult::presented) {
            ++visible_presentations;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
    }

    const game_ex::render::RendererDiagnostics diagnostics = renderer->diagnostics();
    bool passed = check(visible_presentations == required_visible_presentations,
                        "32 Vulkan presentations succeed after the window becomes visible");
    passed &= check(diagnostics.backend == game_ex::render::RendererBackend::vulkan,
                    "diagnostics identify the Vulkan backend");
    passed &= check(diagnostics.api_major > 1U
                        || (diagnostics.api_major == 1U && diagnostics.api_minor >= 3U),
                    "selected device meets the Vulkan 1.3 baseline");
    passed &= check(diagnostics.presentation.find("_SRGB") != std::string::npos
                        && diagnostics.presentation.find("FIFO") != std::string::npos,
                    "diagnostics record sRGB plus FIFO presentation");
    passed &= check(diagnostics.presentation.find("dynamic-rendering raster")
                            != std::string::npos,
                    "diagnostics record the Vulkan 1.3 dynamic-rendering raster path");
    passed &= check(diagnostics.presented_frames >= required_visible_presentations,
                    "backend diagnostics count the visible native presentations");
    passed &= check(diagnostics.last_presented_raster_columns == 2U
                        && diagnostics.last_presented_raster_rows == 2U
                        && diagnostics.last_presented_raster_cell_count == 4U,
                    "diagnostics prove that the complete 2x2 raster was presented");
    passed &=
        check(diagnostics.debug_error_count == 0U, "no Vulkan validation errors were observed");
    if (mode == game_ex::render::VulkanValidationMode::required) {
        passed &= check(diagnostics.debug_diagnostics,
                        "required validation was enabled rather than silently omitted");
    }

    std::cout << "Vulkan " << diagnostics.api_major << '.' << diagnostics.api_minor << '.'
              << diagnostics.api_patch << " | " << diagnostics.device << " | "
              << diagnostics.presentation
              << " | validation=" << (diagnostics.debug_diagnostics ? "enabled" : "disabled")
              << " | raster=" << diagnostics.last_presented_raster_columns << 'x'
              << diagnostics.last_presented_raster_rows << '/'
              << diagnostics.last_presented_raster_cell_count
              << '\n';

    window->hide();
    renderer->shutdown();
    passed &= check(renderer->state() == game_ex::render::RendererLifecycleState::stopped,
                    "Vulkan shutdown enters stopped state");
    return passed;
}

} // namespace

/**
 * @brief Runs the real Vulkan GUI smoke with optional required validation.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return EXIT_SUCCESS only after a verified native presentation.
 */
int main(const int argc, char* argv[]) {
    try {
        return run_smoke(validation_mode(argc, argv)) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "Vulkan smoke failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
