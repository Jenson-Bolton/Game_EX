/**
 * @file renderer.cpp
 * @brief Backend-neutral renderer value and validation implementation.
 */

#include "game_ex/render/renderer.hpp"

#include <array>
#include <cmath>
#include <utility>

namespace game_ex::render {

std::string_view renderer_backend_name(const RendererBackend backend) noexcept {
    switch (backend) {
    case RendererBackend::open_gl:
        return "opengl";
    case RendererBackend::vulkan:
        return "vulkan";
    }

    return "unknown";
}

RendererError::RendererError(
    const RendererErrorCode code,
    const RendererBackend backend,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code), backend_(backend) {}

RendererErrorCode RendererError::code() const noexcept {
    return code_;
}

RendererBackend RendererError::backend() const noexcept {
    return backend_;
}

DiagnosticFrame foundation_diagnostic_frame() noexcept {
    return {
        .red = 0.035F,
        .green = 0.065F,
        .blue = 0.110F,
        .alpha = 1.0F,
    };
}

void validate_diagnostic_frame(const DiagnosticFrame& frame) {
    const std::array components{frame.red, frame.green, frame.blue, frame.alpha};
    for (const float component : components) {
        if (!std::isfinite(component) || component < 0.0F || component > 1.0F) {
            throw std::invalid_argument(
                "Diagnostic frame components must be finite values in [0, 1]");
        }
    }
}

} // namespace game_ex::render
