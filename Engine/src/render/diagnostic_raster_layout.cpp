/**
 * @file diagnostic_raster_layout.cpp
 * @brief Shared integer layout for diagnostic raster backends.
 */

#include "render/diagnostic_raster_layout.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace game_ex::render::detail {
namespace {

/**
 * @brief Returns one floor-partition boundary without narrow multiplication.
 * @param origin First pixel coordinate of the partitioned interval.
 * @param extent Pixel extent of the partitioned interval.
 * @param part Boundary number in the inclusive range [0, part_count].
 * @param part_count Non-zero number of equal logical partitions.
 * @return Bottom- or left-origin integer pixel boundary.
 */
[[nodiscard]] std::uint32_t partition_boundary(
    const std::uint32_t origin,
    const std::uint32_t extent,
    const std::uint32_t part,
    const std::uint32_t part_count) noexcept {
    const std::uint64_t scaled = static_cast<std::uint64_t>(extent) * part;
    return origin + static_cast<std::uint32_t>(scaled / part_count);
}

/**
 * @brief Converts a positive fitted floating extent into a drawable-safe integer.
 * @param extent Positive mathematical fitted extent.
 * @param drawable_extent Non-zero drawable bound.
 * @return Floor of extent clamped to [1, drawable_extent].
 */
[[nodiscard]] std::uint32_t fitted_integer_extent(
    const long double extent,
    const std::uint32_t drawable_extent) noexcept {
    const long double floored = std::floor(extent);
    if (floored < 1.0L) {
        return 1U;
    }
    if (floored >= static_cast<long double>(drawable_extent)) {
        return drawable_extent;
    }
    return static_cast<std::uint32_t>(floored);
}

} // namespace

DiagnosticRasterLayout layout_diagnostic_raster(
    const DiagnosticRaster& raster,
    const std::uint32_t drawable_width,
    const std::uint32_t drawable_height) {
    validate_diagnostic_raster(raster);
    if (drawable_width == 0U || drawable_height == 0U) {
        throw std::invalid_argument(
            "Diagnostic raster layout requires a non-zero drawable extent");
    }

    const long double target_aspect =
        static_cast<long double>(raster.display_aspect_ratio);
    const long double full_height_width =
        static_cast<long double>(drawable_height) * target_aspect;

    std::uint32_t content_width{};
    std::uint32_t content_height{};
    if (full_height_width <= static_cast<long double>(drawable_width)) {
        content_width = fitted_integer_extent(full_height_width, drawable_width);
        content_height = drawable_height;
    } else {
        content_width = drawable_width;
        content_height = fitted_integer_extent(
            static_cast<long double>(drawable_width) / target_aspect,
            drawable_height);
    }

    DiagnosticRasterLayout layout{
        .x = (drawable_width - content_width) / 2U,
        .y = (drawable_height - content_height) / 2U,
        .width = content_width,
        .height = content_height,
        .cells = {},
    };

    const std::uint64_t cell_count =
        static_cast<std::uint64_t>(raster.columns)
        * static_cast<std::uint64_t>(raster.rows);
    layout.cells.reserve(static_cast<std::size_t>(cell_count));

    for (std::uint32_t row = 0U; row < raster.rows; ++row) {
        const std::uint32_t lower = partition_boundary(
            layout.y, layout.height, row, raster.rows);
        const std::uint32_t upper = partition_boundary(
            layout.y, layout.height, row + 1U, raster.rows);
        for (std::uint32_t column = 0U; column < raster.columns; ++column) {
            const std::uint32_t left = partition_boundary(
                layout.x, layout.width, column, raster.columns);
            const std::uint32_t right = partition_boundary(
                layout.x, layout.width, column + 1U, raster.columns);
            const std::uint64_t colour_index =
                static_cast<std::uint64_t>(row) * raster.columns + column;
            layout.cells.push_back({
                .x = left,
                .y = lower,
                .width = right - left,
                .height = upper - lower,
                .colour_index = static_cast<std::uint32_t>(colour_index),
            });
        }
    }

    return layout;
}

} // namespace game_ex::render::detail
