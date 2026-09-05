/**
 * @file renderer_selection_tests.cpp
 * @brief Pure tests for desktop CLI parsing and strict renderer fallback.
 */

#include "renderer_selection.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using game_ex::game::selection_detail::RendererAttemptFailure;
using game_ex::game::selection_detail::RendererAttemptResult;
using game_ex::game::selection_detail::RendererAttemptStage;
using game_ex::game::selection_detail::RendererChoice;
using game_ex::render::RendererBackend;
using game_ex::render::RendererError;
using game_ex::render::RendererErrorCode;

/**
 * @brief Reports one failed assertion without skipping remaining cases.
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
 * @brief Returns whether parsing selected input throws invalid_argument.
 * @param arguments Command-line arguments after the executable name.
 * @return True only for the expected validation exception.
 */
bool parse_is_rejected(const std::span<const std::string_view> arguments) {
    try {
        static_cast<void>(game_ex::game::selection_detail::parse_command_line(arguments));
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

/**
 * @brief Verifies defaulting, order independence, and strict option rejection.
 * @return True when the CLI grammar behaves exactly as documented.
 */
bool test_command_line() {
    const std::array<std::string_view, 0U> none{};
    const auto defaults = game_ex::game::selection_detail::parse_command_line(none);
    const std::array first_order{std::string_view{"--renderer=vulkan"},
                                 std::string_view{"--quit-after-ms=250"}};
    const std::array second_order{std::string_view{"--quit-after-ms=250"},
                                  std::string_view{"--renderer=opengl"}};
    const auto first = game_ex::game::selection_detail::parse_command_line(first_order);
    const auto second = game_ex::game::selection_detail::parse_command_line(second_order);
    const std::array explicit_auto{std::string_view{"--renderer=auto"}};
    const auto parsed_auto = game_ex::game::selection_detail::parse_command_line(explicit_auto);

    bool passed = check(defaults.renderer == RendererChoice::automatic
                            && !defaults.automatic_exit_after.has_value(),
                        "renderer omission defaults to auto");
    passed &= check(first.renderer == RendererChoice::vulkan
                        && first.automatic_exit_after == std::chrono::milliseconds{250},
                    "renderer then quit-after parses");
    passed &= check(second.renderer == RendererChoice::open_gl
                        && second.automatic_exit_after == std::chrono::milliseconds{250},
                    "quit-after then renderer parses");
    passed &= check(parsed_auto.renderer == RendererChoice::automatic,
                    "explicit auto spelling selects automatic policy");

    const std::array empty_renderer{std::string_view{"--renderer="}};
    const std::array unknown_renderer{std::string_view{"--renderer=metal"}};
    const std::array repeated_renderer{std::string_view{"--renderer=vulkan"},
                                       std::string_view{"--renderer=vulkan"}};
    const std::array conflicting_renderer{std::string_view{"--renderer=vulkan"},
                                          std::string_view{"--renderer=opengl"}};
    const std::array empty_duration{std::string_view{"--quit-after-ms="}};
    const std::array signed_duration{std::string_view{"--quit-after-ms=-1"}};
    const std::array excessive_duration{std::string_view{"--quit-after-ms=86400001"}};
    const std::array repeated_duration{std::string_view{"--quit-after-ms=1"},
                                       std::string_view{"--quit-after-ms=2"}};
    const std::array unknown_option{std::string_view{"--unknown"}};
    passed &= check(parse_is_rejected(empty_renderer), "empty renderer is rejected");
    passed &= check(parse_is_rejected(unknown_renderer), "unknown renderer is rejected");
    passed &= check(parse_is_rejected(repeated_renderer), "repeated renderer is rejected");
    passed &= check(parse_is_rejected(conflicting_renderer), "conflicting renderer is rejected");
    passed &= check(parse_is_rejected(empty_duration), "empty duration is rejected");
    passed &= check(parse_is_rejected(signed_duration), "signed duration is rejected");
    passed &= check(parse_is_rejected(excessive_duration), "excessive duration is rejected");
    passed &= check(parse_is_rejected(repeated_duration), "repeated duration is rejected");
    passed &= check(parse_is_rejected(unknown_option), "unknown option is rejected");
    return passed;
}

/**
 * @brief Builds one injected failed renderer result.
 * @param code Stable error category.
 * @param backend Backend reported by the failure.
 * @param stage Visibility phase in which it escaped.
 * @return Attempt result retaining exact failure facts.
 */
RendererAttemptResult failed(const RendererErrorCode code, const RendererBackend backend,
                             const RendererAttemptStage stage) {
    return {
        .exit_status = 1,
        .failure =
            RendererAttemptFailure{
                .code = code,
                .backend = backend,
                .stage = stage,
                .message = "injected failure",
            },
    };
}

/**
 * @brief Runs selection and captures an expected renderer error code.
 * @param choice Renderer choice under test.
 * @param attempt Injected attempt behaviour.
 * @return Caught renderer error code, or empty if selection returned normally.
 */
std::optional<RendererErrorCode>
caught_code(const RendererChoice choice,
            const game_ex::game::selection_detail::RendererAttempt& attempt) {
    try {
        static_cast<void>(
            game_ex::game::selection_detail::execute_renderer_selection(choice, attempt));
    } catch (const RendererError& error) {
        return error.code();
    }
    return std::nullopt;
}

/**
 * @brief Verifies exact attempt order and the narrow automatic fallback gate.
 * @return True when no explicit or post-visibility failure can fall back
 * silently.
 */
bool test_selection_policy() {
    bool passed = true;

    std::vector<RendererBackend> attempts;
    const int direct_status = game_ex::game::selection_detail::execute_renderer_selection(
        RendererChoice::open_gl, [&attempts](const RendererBackend backend) {
            attempts.push_back(backend);
            return RendererAttemptResult{.exit_status = 7, .failure = std::nullopt};
        });
    passed &= check(direct_status == 7 && attempts == std::vector{RendererBackend::open_gl},
                    "explicit OpenGL performs exactly one OpenGL attempt");

    attempts.clear();
    const auto explicit_vulkan_code =
        caught_code(RendererChoice::vulkan, [&attempts](const RendererBackend backend) {
            attempts.push_back(backend);
            return failed(RendererErrorCode::initialization_failed, backend,
                          RendererAttemptStage::before_visibility);
        });
    passed &= check(explicit_vulkan_code == RendererErrorCode::initialization_failed
                        && attempts == std::vector{RendererBackend::vulkan},
                    "explicit Vulkan never falls back");

    attempts.clear();
    bool unexpected_observer{};
    const int automatic_vulkan_status = game_ex::game::selection_detail::execute_renderer_selection(
        RendererChoice::automatic,
        [&attempts](const RendererBackend backend) {
            attempts.push_back(backend);
            return RendererAttemptResult{.exit_status = 4, .failure = std::nullopt};
        },
        [&unexpected_observer](const RendererAttemptFailure&) { unexpected_observer = true; });
    passed &= check(automatic_vulkan_status == 4 && attempts == std::vector{RendererBackend::vulkan}
                        && !unexpected_observer,
                    "successful automatic Vulkan performs one attempt and no "
                    "fallback notification");

    for (const RendererErrorCode eligible :
         {RendererErrorCode::unavailable, RendererErrorCode::initialization_failed}) {
        attempts.clear();
        std::optional<RendererAttemptFailure> observed_fallback;
        const int status = game_ex::game::selection_detail::execute_renderer_selection(
            RendererChoice::automatic,
            [&attempts, eligible](const RendererBackend backend) {
                attempts.push_back(backend);
                if (backend == RendererBackend::vulkan) {
                    return failed(eligible, backend, RendererAttemptStage::before_visibility);
                }
                return RendererAttemptResult{.exit_status = 0, .failure = std::nullopt};
            },
            [&observed_fallback](const RendererAttemptFailure& failure) {
                observed_fallback = failure;
            });
        passed &=
            check(status == 0
                      && attempts == std::vector{RendererBackend::vulkan, RendererBackend::open_gl},
                  "auto tries Vulkan then fresh OpenGL for an eligible hidden failure");
        passed &= check(observed_fallback.has_value() && observed_fallback->code == eligible
                            && observed_fallback->message == "injected failure",
                        "eligible fallback preserves the preferred backend diagnostic");
    }

    for (const auto scenario :
         {std::pair{RendererErrorCode::presentation_failed,
                    RendererAttemptStage::before_visibility},
          std::pair{RendererErrorCode::unavailable, RendererAttemptStage::after_visibility},
          std::pair{RendererErrorCode::initialization_failed,
                    RendererAttemptStage::after_visibility},
          std::pair{RendererErrorCode::presentation_failed, RendererAttemptStage::after_visibility},
          std::pair{RendererErrorCode::shutdown_failed, RendererAttemptStage::after_visibility}}) {
        attempts.clear();
        const auto code = caught_code(RendererChoice::automatic,
                                      [&attempts, scenario](const RendererBackend backend) {
                                          attempts.push_back(backend);
                                          return failed(scenario.first, backend, scenario.second);
                                      });
        passed &= check(code == scenario.first && attempts == std::vector{RendererBackend::vulkan},
                        "ineligible or post-visibility error cannot trigger fallback");
    }

    attempts.clear();
    const auto inconsistent_backend =
        caught_code(RendererChoice::automatic, [&attempts](const RendererBackend backend) {
            attempts.push_back(backend);
            return failed(RendererErrorCode::unavailable, RendererBackend::open_gl,
                          RendererAttemptStage::before_visibility);
        });
    passed &= check(inconsistent_backend == RendererErrorCode::unavailable
                        && attempts == std::vector{RendererBackend::vulkan},
                    "inconsistent backend evidence cannot authorize fallback");

    attempts.clear();
    std::optional<RendererAttemptFailure> preferred_failure;
    std::optional<RendererErrorCode> fallback_failure;
    try {
        static_cast<void>(game_ex::game::selection_detail::execute_renderer_selection(
            RendererChoice::automatic,
            [&attempts](const RendererBackend backend) {
                attempts.push_back(backend);
                if (backend == RendererBackend::vulkan) {
                    return failed(RendererErrorCode::unavailable, backend,
                                  RendererAttemptStage::before_visibility);
                }
                return failed(RendererErrorCode::initialization_failed, backend,
                              RendererAttemptStage::before_visibility);
            },
            [&preferred_failure](const RendererAttemptFailure& failure) {
                preferred_failure = failure;
            }));
    } catch (const RendererError& error) {
        fallback_failure = error.code();
    }
    passed &= check(fallback_failure == RendererErrorCode::initialization_failed,
                    "OpenGL fallback failure is propagated");
    passed &=
        check(preferred_failure.has_value() && preferred_failure->backend == RendererBackend::vulkan
                  && attempts == std::vector{RendererBackend::vulkan, RendererBackend::open_gl},
              "failed fallback retains Vulkan cause and makes exactly two attempts");

    bool rejected_empty_attempt = false;
    try {
        static_cast<void>(game_ex::game::selection_detail::execute_renderer_selection(
            RendererChoice::automatic, {}));
    } catch (const std::invalid_argument&) {
        rejected_empty_attempt = true;
    }
    passed &= check(rejected_empty_attempt, "empty attempt callable is rejected");
    return passed;
}

} // namespace

/**
 * @brief Runs renderer command-line and fallback-policy unit tests.
 * @return EXIT_SUCCESS when every policy assertion passes.
 */
int main() {
    bool passed = true;
    passed &= test_command_line();
    passed &= test_selection_policy();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
