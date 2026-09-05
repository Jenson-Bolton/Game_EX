/**
 * @file opengl_renderer_smoke.cpp
 * @brief Real SDL/OpenGL 4.6 Core diagnostic-frame smoke test.
 */

#include "game_ex/platform/sdl_platform.hpp"
#include "game_ex/render/opengl_renderer.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

/**
 * @brief Reports one failed assertion without aborting cleanup.
 * @param condition Assertion result.
 * @param description Human-readable invariant.
 * @return True when the assertion passed.
 */
bool check(const bool condition, const std::string& description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
    }
    return condition;
}

/**
 * @brief Executes the real backend lifecycle against a small desktop window.
 * @return True when OpenGL 4.6 Core clears and presents without API errors.
 */
bool run_smoke() {
    auto platform = game_ex::platform::create_sdl_platform({
        .application_name = "Game_EX OpenGL smoke",
        .application_version = GAMEEX_VERSION_STRING,
        .application_identifier = "dev.game-ex.tests.opengl-smoke",
    });
    auto factory = game_ex::render::create_opengl_renderer_factory();

    game_ex::platform::WindowSpecification specification{
        .title = "Game_EX OpenGL 4.6 diagnostic smoke",
        .width = 320U,
        .height = 200U,
        .resizable = false,
        .graphics_api = factory->required_window_api(),
    };
    auto window = platform->create_window(specification);
    auto renderer = factory->create(*window);

    bool passed = check(
        renderer->state() == game_ex::render::RendererLifecycleState::dormant,
        "new OpenGL renderer is dormant");
    try {
        static_cast<void>(renderer->render_frame(
            game_ex::render::foundation_render_frame()));
        passed &= check(false, "render before start is rejected");
    } catch (const game_ex::render::RendererError& error) {
        passed &= check(
            error.code() == game_ex::render::RendererErrorCode::invalid_state,
            "render-before-start reports invalid state");
    }

    renderer->start();
    passed &= check(
        renderer->state() == game_ex::render::RendererLifecycleState::running,
        "successful OpenGL startup enters running state");

    const game_ex::render::RendererDiagnostics& diagnostics = renderer->diagnostics();
    passed &= check(
        diagnostics.backend == game_ex::render::RendererBackend::open_gl,
        "diagnostics identify OpenGL backend");
    passed &= check(
        diagnostics.api_major > 4U
            || (diagnostics.api_major == 4U && diagnostics.api_minor >= 6U),
        "actual OpenGL version meets 4.6 baseline");
    passed &= check(diagnostics.profile == "core", "actual OpenGL profile is core");
    passed &= check(!diagnostics.api_version.empty(), "OpenGL version text is present");
    passed &= check(!diagnostics.vendor.empty(), "OpenGL vendor text is present");
    passed &= check(!diagnostics.device.empty(), "OpenGL device text is present");

    std::cout << "OpenGL " << diagnostics.api_major << '.' << diagnostics.api_minor
              << " core | " << diagnostics.vendor << " | " << diagnostics.device << '\n';

    try {
        static_cast<void>(renderer->render_frame({
            .background = {
                std::numeric_limits<float>::quiet_NaN(),
                0.0F,
                0.0F,
                1.0F},
            .raster = std::nullopt,
        }));
        passed &= check(false, "non-finite real-backend frame is rejected");
    } catch (const std::invalid_argument&) {
        passed &= check(true, "non-finite real-backend frame is rejected");
    }

    static_cast<void>(renderer->render_frame(
        game_ex::render::foundation_render_frame()));
    window->show();
    const game_ex::render::RenderFrame raster_frame{
        .background = {0.02F, 0.02F, 0.02F, 1.0F},
        .raster = game_ex::render::DiagnosticRaster{
            .columns = 3U,
            .rows = 2U,
            .display_aspect_ratio = 1.5F,
            .linear_colours = {
                {1.0F, 0.0F, 0.0F, 1.0F},
                {0.0F, 1.0F, 0.0F, 1.0F},
                {0.0F, 0.0F, 1.0F, 1.0F},
                {0.0F, 1.0F, 1.0F, 1.0F},
                {1.0F, 0.0F, 1.0F, 1.0F},
                {1.0F, 1.0F, 0.0F, 1.0F},
            },
        },
    };
    const auto visible_result = renderer->render_frame(
        raster_frame);
    passed &= check(
        visible_result == game_ex::render::FramePresentationResult::presented,
        "OpenGL presents an aspect-fitted raster after the window becomes visible");
    passed &= check(
        diagnostics.last_presented_raster_columns == 3U
            && diagnostics.last_presented_raster_rows == 2U
            && diagnostics.last_presented_raster_cell_count == 6U,
        "OpenGL diagnostics retain the last presented logical raster dimensions");

    static_cast<void>(renderer->render_frame(
        game_ex::render::foundation_render_frame()));
    passed &= check(
        diagnostics.last_presented_raster_columns == 0U
            && diagnostics.last_presented_raster_rows == 0U
            && diagnostics.last_presented_raster_cell_count == 0U,
        "raster-free presentation resets last-presented raster diagnostics");
    static_cast<void>(platform->pump_events());
    window->hide();
    renderer->shutdown();

    passed &= check(
        renderer->state() == game_ex::render::RendererLifecycleState::stopped,
        "OpenGL shutdown enters stopped state");
    try {
        renderer->shutdown();
        passed &= check(false, "second OpenGL shutdown is rejected");
    } catch (const game_ex::render::RendererError& error) {
        passed &= check(
            error.code() == game_ex::render::RendererErrorCode::invalid_state,
            "second shutdown reports invalid state");
    }

    return passed;
}

} // namespace

/**
 * @brief Runs the real OpenGL GUI smoke test.
 * @return EXIT_SUCCESS after a verified clear/present lifecycle.
 */
int main() {
    try {
        return run_smoke() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "OpenGL smoke failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
