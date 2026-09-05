/**
 * @file renderer_selection.cpp
 * @brief Renderer command-line parsing and strict automatic-fallback policy.
 */

#include "renderer_selection.hpp"

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <system_error>

namespace game_ex::game::selection_detail {
namespace {

/** Prefix for the public renderer-selection option. */
constexpr std::string_view renderer_prefix{"--renderer="};

/** Prefix for the automation-only window lifetime option. */
constexpr std::string_view quit_after_prefix{"--quit-after-ms="};

/** Maximum accepted automated lifetime of 24 hours. */
constexpr std::uint64_t maximum_duration_ms{86'400'000U};

/**
 * @brief Converts an exact renderer spelling to its internal choice.
 * @param value Text after `--renderer=`.
 * @return Matching renderer choice.
 * @throws std::invalid_argument for empty or unknown values.
 */
[[nodiscard]] RendererChoice parse_renderer_value(const std::string_view value) {
    if (value == "auto") {
        return RendererChoice::automatic;
    }
    if (value == "opengl") {
        return RendererChoice::open_gl;
    }
    if (value == "vulkan") {
        return RendererChoice::vulkan;
    }
    throw std::invalid_argument("--renderer requires exactly one of: auto, opengl, vulkan");
}

/**
 * @brief Parses and bounds one automatic-exit millisecond value.
 * @param value Text after `--quit-after-ms=`.
 * @return Non-negative bounded duration.
 * @throws std::invalid_argument for empty, signed, non-numeric, or excessive
 * input.
 */
[[nodiscard]] std::chrono::milliseconds parse_duration(const std::string_view value) {
    std::uint64_t milliseconds{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), milliseconds);
    if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::invalid_argument("--quit-after-ms requires an unsigned integer");
    }
    if (milliseconds > maximum_duration_ms) {
        throw std::invalid_argument("--quit-after-ms cannot exceed 24 hours");
    }
    return std::chrono::milliseconds{milliseconds};
}

/**
 * @brief Recreates and throws the typed error retained in an attempt result.
 * @param failure Failure facts to propagate.
 * @throws render::RendererError unconditionally.
 */
[[noreturn]] void throw_attempt_failure(const RendererAttemptFailure& failure) {
    throw render::RendererError{failure.code, failure.backend, failure.message};
}

/**
 * @brief Invokes one attempt and either returns status or propagates failure.
 * @param backend Explicit backend to attempt.
 * @param attempt Injected fresh-composition callable.
 * @return Normal process exit status.
 * @throws render::RendererError when the attempt reports failure.
 */
[[nodiscard]] int execute_explicit(const render::RendererBackend backend,
                                   const RendererAttempt& attempt) {
    const RendererAttemptResult result = attempt(backend);
    if (result.failure.has_value()) {
        throw_attempt_failure(result.failure.value());
    }
    return result.exit_status;
}

} // namespace

DesktopCommandLine parse_command_line(const std::span<const std::string_view> arguments) {
    DesktopCommandLine parsed{};
    bool saw_renderer{};
    bool saw_quit_after{};

    for (const std::string_view argument : arguments) {
        if (argument.starts_with(renderer_prefix)) {
            if (saw_renderer) {
                throw std::invalid_argument("--renderer may be supplied only once");
            }
            saw_renderer = true;
            parsed.renderer = parse_renderer_value(argument.substr(renderer_prefix.size()));
            continue;
        }

        if (argument.starts_with(quit_after_prefix)) {
            if (saw_quit_after) {
                throw std::invalid_argument("--quit-after-ms may be supplied only once");
            }
            saw_quit_after = true;
            parsed.automatic_exit_after = parse_duration(argument.substr(quit_after_prefix.size()));
            continue;
        }

        throw std::invalid_argument("Unknown command-line argument: " + std::string{argument});
    }
    return parsed;
}

int execute_renderer_selection(const RendererChoice choice, const RendererAttempt& attempt,
                               const RendererFallbackObserver& fallback_observer) {
    if (!attempt) {
        throw std::invalid_argument("Renderer selection requires an attempt callable");
    }
    if (choice == RendererChoice::open_gl) {
        return execute_explicit(render::RendererBackend::open_gl, attempt);
    }
    if (choice == RendererChoice::vulkan) {
        return execute_explicit(render::RendererBackend::vulkan, attempt);
    }

    const RendererAttemptResult vulkan_result = attempt(render::RendererBackend::vulkan);
    if (!vulkan_result.failure.has_value()) {
        return vulkan_result.exit_status;
    }

    const RendererAttemptFailure& failure = vulkan_result.failure.value();
    const bool eligible_category =
        failure.code == render::RendererErrorCode::unavailable
        || failure.code == render::RendererErrorCode::initialization_failed;
    if (failure.backend != render::RendererBackend::vulkan
        || failure.stage != RendererAttemptStage::before_visibility || !eligible_category) {
        throw_attempt_failure(failure);
    }
    if (fallback_observer) {
        fallback_observer(failure);
    }
    return execute_explicit(render::RendererBackend::open_gl, attempt);
}

} // namespace game_ex::game::selection_detail
