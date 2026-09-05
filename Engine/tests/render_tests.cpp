/**
 * @file render_tests.cpp
 * @brief Dependency-free tests for the backend-neutral render contract.
 */

#include "game_ex/render/renderer.hpp"

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

/**
 * @brief Reports one failed assertion without aborting the test executable.
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
 * @brief Tests whether invoking a callable raises a selected exception type.
 * @tparam Exception Expected exception base or exact type.
 * @tparam Callable Nullary callable type.
 * @param callable Operation under test.
 * @return True only when the selected exception type was caught.
 */
template <typename Exception, typename Callable>
bool throws_exception(Callable&& callable) {
    try {
        std::invoke(std::forward<Callable>(callable));
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

/**
 * @brief Verifies stable backend names and categorized renderer errors.
 * @return True when neutral diagnostic values retain their contract.
 */
bool test_diagnostic_values() {
    using game_ex::render::RendererBackend;
    using game_ex::render::RendererError;
    using game_ex::render::RendererErrorCode;

    bool passed = true;
    passed &= check(
        game_ex::render::renderer_backend_name(RendererBackend::open_gl) == "opengl",
        "OpenGL backend name is a stable lowercase value");
    passed &= check(
        game_ex::render::renderer_backend_name(RendererBackend::vulkan) == "vulkan",
        "Vulkan backend name is a stable lowercase value");

    const RendererError error{
        RendererErrorCode::presentation_failed,
        RendererBackend::open_gl,
        "present failed"};
    passed &= check(
        error.code() == RendererErrorCode::presentation_failed,
        "renderer error retains its stable category");
    passed &= check(
        error.backend() == RendererBackend::open_gl,
        "renderer error retains its backend");
    passed &= check(
        std::string{error.what()} == "present failed",
        "renderer error retains its human-readable detail");
    return passed;
}

/**
 * @brief Verifies finite inclusive clear-colour validation.
 * @return True when valid endpoints pass and invalid values are rejected.
 */
bool test_frame_validation() {
    using game_ex::render::DiagnosticFrame;
    using game_ex::render::validate_diagnostic_frame;

    bool passed = true;
    validate_diagnostic_frame({0.0F, 0.0F, 0.0F, 0.0F});
    validate_diagnostic_frame({1.0F, 1.0F, 1.0F, 1.0F});
    const DiagnosticFrame foundation = game_ex::render::foundation_diagnostic_frame();
    validate_diagnostic_frame(foundation);
    passed &= check(
        foundation.red == 0.035F
            && foundation.green == 0.065F
            && foundation.blue == 0.110F
            && foundation.alpha == 1.0F,
        "foundation diagnostic frame retains its exact cross-backend values");

    const std::array invalid_values{
        -0.001F,
        1.001F,
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
    };

    for (const float value : invalid_values) {
        passed &= check(
            throws_exception<std::invalid_argument>([value] {
                game_ex::render::validate_diagnostic_frame(
                    DiagnosticFrame{value, 0.5F, 0.5F, 1.0F});
            }),
            "invalid red component is rejected");
        passed &= check(
            throws_exception<std::invalid_argument>([value] {
                game_ex::render::validate_diagnostic_frame(
                    DiagnosticFrame{0.5F, value, 0.5F, 1.0F});
            }),
            "invalid green component is rejected");
        passed &= check(
            throws_exception<std::invalid_argument>([value] {
                game_ex::render::validate_diagnostic_frame(
                    DiagnosticFrame{0.5F, 0.5F, value, 1.0F});
            }),
            "invalid blue component is rejected");
        passed &= check(
            throws_exception<std::invalid_argument>([value] {
                game_ex::render::validate_diagnostic_frame(
                    DiagnosticFrame{0.5F, 0.5F, 0.5F, value});
            }),
            "invalid alpha component is rejected");
    }

    return passed;
}

} // namespace

/**
 * @brief Runs all backend-neutral render API tests.
 * @return EXIT_SUCCESS when every contract invariant holds.
 */
int main() {
    bool passed = true;
    passed &= test_diagnostic_values();
    passed &= test_frame_validation();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
