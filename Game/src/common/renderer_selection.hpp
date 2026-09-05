/**
 * @file renderer_selection.hpp
 * @brief Pure command-line and renderer-attempt selection contracts.
 */

#pragma once

#include "game_ex/render/renderer.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace game_ex::game::selection_detail {

/**
 * @brief User-facing renderer selection, including ordered automatic probing.
 */
enum class RendererChoice {
    /** Try Vulkan first, then OpenGL only after an eligible hidden failure. */
    automatic,

    /** Require OpenGL without fallback. */
    open_gl,

    /** Require Vulkan without fallback. */
    vulkan
};

/**
 * @brief Fully parsed desktop command-line configuration.
 */
struct DesktopCommandLine final {
    /** Renderer choice; omission intentionally means automatic selection. */
    RendererChoice renderer{RendererChoice::automatic};

    /** Optional automation-only window lifetime. */
    std::optional<std::chrono::milliseconds> automatic_exit_after;
};

/**
 * @brief Parses order-independent desktop application options.
 * @param arguments Arguments after the executable name.
 * @return Validated renderer and automatic-exit configuration.
 * @throws std::invalid_argument for unknown, empty, repeated, or malformed
 * options.
 */
[[nodiscard]] DesktopCommandLine parse_command_line(std::span<const std::string_view> arguments);

/**
 * @brief Indicates whether a renderer failure occurred before window
 * visibility.
 */
enum class RendererAttemptStage {
    /** No selected-backend window became visible. */
    before_visibility,

    /** Visibility completed, so fallback would hide a runtime failure. */
    after_visibility
};

/**
 * @brief Serializable facts describing one failed backend attempt.
 */
struct RendererAttemptFailure final {
    /** Stable renderer failure category. */
    render::RendererErrorCode code{render::RendererErrorCode::initialization_failed};

    /** Backend whose attempt failed. */
    render::RendererBackend backend{render::RendererBackend::open_gl};

    /** Whether the application had crossed its visibility boundary. */
    RendererAttemptStage stage{RendererAttemptStage::before_visibility};

    /** Human-readable cause preserved when rethrowing. */
    std::string message;
};

/**
 * @brief Outcome returned by an injected renderer attempt.
 */
struct RendererAttemptResult final {
    /** Process status when the attempt completed normally. */
    int exit_status{};

    /** Failure facts when the attempt did not complete. */
    std::optional<RendererAttemptFailure> failure;
};

/** Callable used to run one completely fresh backend composition attempt. */
using RendererAttempt = std::function<RendererAttemptResult(render::RendererBackend)>;

/** Callback invoked immediately before one eligible automatic fallback. */
using RendererFallbackObserver = std::function<void(const RendererAttemptFailure&)>;

/**
 * @brief Executes explicit or automatic renderer selection with strict
 * fallback.
 * @param choice Parsed renderer choice.
 * @param attempt Callable that creates a fresh Platform and Application per
 * call.
 * @param fallback_observer Optional diagnostic observer for the preserved
 * Vulkan failure.
 * @return Exit status from the selected successful attempt.
 * @throws std::invalid_argument if attempt is empty.
 * @throws render::RendererError for explicit failures, OpenGL fallback
 * failures, or any Vulkan failure after visibility or outside the two eligible
 * categories.
 */
int execute_renderer_selection(RendererChoice choice, const RendererAttempt& attempt,
                               const RendererFallbackObserver& fallback_observer = {});

} // namespace game_ex::game::selection_detail
