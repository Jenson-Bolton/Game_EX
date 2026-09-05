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
        renderer->render_frame(game_ex::render::foundation_diagnostic_frame());
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
        renderer->render_frame({
            std::numeric_limits<float>::quiet_NaN(),
            0.0F,
            0.0F,
            1.0F});
        passed &= check(false, "non-finite real-backend frame is rejected");
    } catch (const std::invalid_argument&) {
        passed &= check(true, "non-finite real-backend frame is rejected");
    }

    renderer->render_frame(game_ex::render::foundation_diagnostic_frame());
    window->show();
    renderer->render_frame(game_ex::render::foundation_diagnostic_frame());
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
