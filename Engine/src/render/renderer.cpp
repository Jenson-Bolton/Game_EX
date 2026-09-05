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

RenderFrame foundation_render_frame() noexcept {
    return {
        .background = foundation_diagnostic_frame(),
        .raster = std::nullopt,
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

void validate_diagnostic_raster(const DiagnosticRaster& raster) {
    if (raster.columns == 0U
        || raster.columns > maximum_diagnostic_raster_dimension
        || raster.rows == 0U
        || raster.rows > maximum_diagnostic_raster_dimension) {
        throw std::invalid_argument(
            "Diagnostic raster dimensions must each be in [1, 64]");
    }

    if (!std::isfinite(raster.display_aspect_ratio)
        || raster.display_aspect_ratio <= 0.0F) {
        throw std::invalid_argument(
            "Diagnostic raster display aspect ratio must be finite and positive");
    }

    const std::uint64_t expected_cells =
        static_cast<std::uint64_t>(raster.columns)
        * static_cast<std::uint64_t>(raster.rows);
    if (expected_cells > maximum_diagnostic_raster_cells
        || raster.linear_colours.size() != static_cast<std::size_t>(expected_cells)) {
        throw std::invalid_argument(
            "Diagnostic raster colour count must equal columns times rows and not exceed 4096");
    }

    for (const DiagnosticFrame& colour : raster.linear_colours) {
        validate_diagnostic_frame(colour);
    }
}

void validate_render_frame(const RenderFrame& frame) {
    validate_diagnostic_frame(frame.background);
    if (frame.raster.has_value()) {
        validate_diagnostic_raster(frame.raster.value());
    }
}

} // namespace game_ex::render
