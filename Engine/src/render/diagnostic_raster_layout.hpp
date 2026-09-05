/**
 * @file diagnostic_raster_layout.hpp
 * @brief Shared integer layout for diagnostic raster backends.
 */

#pragma once

#include "game_ex/render/renderer.hpp"

#include <cstdint>
#include <vector>

namespace game_ex::render::detail {

/**
 * @brief One bottom-origin pixel rectangle and its row-major raster index.
 */
struct DiagnosticRasterRectangle final {
    /** Horizontal pixel offset from the drawable's left edge. */
    std::uint32_t x{};

    /** Vertical pixel offset from the drawable's lower edge. */
    std::uint32_t y{};

    /** Rectangle width in pixels; may be zero when cells outnumber pixels. */
    std::uint32_t width{};

    /** Rectangle height in pixels; may be zero when cells outnumber pixels. */
    std::uint32_t height{};

    /** Index into DiagnosticRaster::linear_colours. */
    std::uint32_t colour_index{};
};

/**
 * @brief Aspect-fitted content bounds and exact row-major cell partition.
 */
struct DiagnosticRasterLayout final {
    /** Horizontal content offset from the drawable's left edge. */
    std::uint32_t x{};

    /** Vertical content offset from the drawable's lower edge. */
    std::uint32_t y{};

    /** Aspect-fitted content width in pixels. */
    std::uint32_t width{};

    /** Aspect-fitted content height in pixels. */
    std::uint32_t height{};

    /** Bottom-origin rectangles in the raster's row-major colour order. */
    std::vector<DiagnosticRasterRectangle> cells;
};

/**
 * @brief Aspect-fits and partitions a validated raster into drawable pixels.
 * @param raster Validated raster whose dimensions and display aspect define layout.
 * @param drawable_width Non-zero drawable width in pixels.
 * @param drawable_height Non-zero drawable height in pixels.
 * @return Centred content bounds and one integer rectangle per logical cell.
 * @throws std::invalid_argument if the raster or drawable dimensions are invalid.
 * @throws std::bad_alloc if rectangle storage cannot be allocated.
 *
 * Partition products are evaluated as 64-bit integers. Odd letterbox remainders
 * place the spare pixel on the upper or right edge. Row zero starts at the
 * returned lower content edge.
 */
[[nodiscard]] DiagnosticRasterLayout layout_diagnostic_raster(
    const DiagnosticRaster& raster,
    std::uint32_t drawable_width,
    std::uint32_t drawable_height);

} // namespace game_ex::render::detail
