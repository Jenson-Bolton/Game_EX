/**
 * @file world_editor_model.cpp
 * @brief Deterministic world-package parsing and diagnostic-raster preparation.
 */

#include "world_editor_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace game_ex::game::world_editor_detail {
namespace {

/** Exact prefix for the editor-only world-package option. */
constexpr std::string_view world_prefix{"--world="};

/** Linear colour assigned to a bin containing no valid samples. */
constexpr render::DiagnosticFrame invalid_colour{
    .red = 1.0F,
    .green = 0.0F,
    .blue = 1.0F,
    .alpha = 1.0F,
};

/**
 * @brief Returns the exact source sample count after structural validation.
 * @param terrain In-memory terrain tile to inspect.
 * @return Product of its non-zero dimensions.
 * @throws std::invalid_argument for an inconsistent in-memory tile.
 */
[[nodiscard]] std::size_t validate_terrain_structure(
    const world_format::TerrainTile& terrain) {
    if (terrain.columns == 0U || terrain.rows == 0U) {
        throw std::invalid_argument("World visualization requires non-zero terrain dimensions");
    }

    const std::uint64_t sample_count =
        static_cast<std::uint64_t>(terrain.columns)
        * static_cast<std::uint64_t>(terrain.rows);
    if (sample_count > static_cast<std::uint64_t>(
                           std::numeric_limits<std::size_t>::max())
        || terrain.heights.size() != static_cast<std::size_t>(sample_count)
        || terrain.validity.size() != static_cast<std::size_t>(sample_count)) {
        throw std::invalid_argument(
            "World visualization requires one height and validity byte per terrain sample");
    }

    if (!std::isfinite(terrain.origin_x) || !std::isfinite(terrain.origin_y)
        || !std::isfinite(terrain.origin_z)
        || !std::isfinite(terrain.spacing_x) || terrain.spacing_x <= 0.0
        || !std::isfinite(terrain.spacing_y) || terrain.spacing_y <= 0.0) {
        throw std::invalid_argument(
            "World visualization requires finite origins and positive finite spacing");
    }

    for (std::size_t index = 0; index < static_cast<std::size_t>(sample_count); ++index) {
        if (!std::isfinite(terrain.heights[index]) || terrain.validity[index] > 1U) {
            throw std::invalid_argument(
                "World visualization received an invalid height or validity value");
        }
    }
    return static_cast<std::size_t>(sample_count);
}

/**
 * @brief Maps a normalized valid height to the linear blue-green-yellow ramp.
 * @param normalized Height in the inclusive range [0, 1].
 * @return Opaque linear RGBA ramp colour.
 */
[[nodiscard]] render::DiagnosticFrame height_colour(const float normalized) noexcept {
    if (normalized <= 0.5F) {
        const float fraction = normalized * 2.0F;
        return {
            .red = 0.0F,
            .green = fraction,
            .blue = 1.0F - fraction,
            .alpha = 1.0F,
        };
    }

    const float fraction = (normalized - 0.5F) * 2.0F;
    return {
        .red = fraction,
        .green = 1.0F,
        .blue = 0.0F,
        .alpha = 1.0F,
    };
}

/**
 * @brief Blends a valid-height colour toward magenta by invalid coverage.
 * @param valid Valid-height colour for the bin mean.
 * @param valid_fraction Fraction of bin samples marked valid.
 * @return Opaque linear coverage blend.
 */
[[nodiscard]] render::DiagnosticFrame blend_validity(
    const render::DiagnosticFrame& valid,
    const float valid_fraction) noexcept {
    const float invalid_fraction = 1.0F - valid_fraction;
    return {
        .red = valid.red * valid_fraction + invalid_colour.red * invalid_fraction,
        .green = valid.green * valid_fraction + invalid_colour.green * invalid_fraction,
        .blue = valid.blue * valid_fraction + invalid_colour.blue * invalid_fraction,
        .alpha = 1.0F,
    };
}

/**
 * @brief Computes one floor-division partition boundary.
 * @param output_index Boundary index in [0, output_dimension].
 * @param source_dimension Full source dimension.
 * @param output_dimension Bounded output dimension.
 * @return Source index assigned to this boundary.
 */
[[nodiscard]] std::uint32_t bin_boundary(
    const std::uint32_t output_index,
    const std::uint32_t source_dimension,
    const std::uint32_t output_dimension) noexcept {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(output_index)
         * static_cast<std::uint64_t>(source_dimension))
        / static_cast<std::uint64_t>(output_dimension));
}

/**
 * @brief Computes the half-cell-padded sample-footprint display aspect.
 * @param terrain Validated terrain tile.
 * @return Finite positive raster-footprint aspect representable by Render.
 * @throws std::invalid_argument when the aspect cannot be represented as float.
 *
 * Package numeric bounds describe sample centres and therefore span
 * `(dimension - 1) * spacing`. The conceptual unreduced raster gives each
 * source sample one complete cell centred on that coordinate, extending half a
 * spacing beyond both outer centres. Its footprint is consequently
 * `dimension * spacing`; reduced display cells summarize source partitions
 * while retaining that complete footprint. This also gives singleton axes a
 * non-zero visible extent.
 */
[[nodiscard]] float display_aspect_ratio(const world_format::TerrainTile& terrain) {
    const long double width = static_cast<long double>(terrain.columns)
        * static_cast<long double>(terrain.spacing_x);
    const long double height = static_cast<long double>(terrain.rows)
        * static_cast<long double>(terrain.spacing_y);
    const long double aspect = width / height;
    if (!std::isfinite(aspect) || aspect <= 0.0L
        || aspect > static_cast<long double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(
            "World visualization aspect ratio is not representable");
    }
    const float result = static_cast<float>(aspect);
    if (!std::isfinite(result) || result <= 0.0F) {
        throw std::invalid_argument(
            "World visualization aspect ratio is not representable");
    }
    return result;
}

/**
 * @brief Formats the package CRC-32 as exactly eight lowercase hex digits.
 * @param checksum Stored and verified package checksum.
 * @return Stable hexadecimal text without a numeric prefix.
 */
[[nodiscard]] std::string checksum_text(const std::uint32_t checksum) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(8) << checksum;
    return text.str();
}

} // namespace

WorldEditorCommandLine parse_world_editor_command_line(
    const std::span<const std::string_view> arguments) {
    WorldEditorCommandLine parsed{};
    parsed.desktop_arguments.reserve(arguments.size());

    for (const std::string_view argument : arguments) {
        if (!argument.starts_with(world_prefix)) {
            parsed.desktop_arguments.push_back(argument);
            continue;
        }
        if (parsed.world_path.has_value()) {
            throw std::invalid_argument("--world may be supplied only once");
        }

        const std::string_view value = argument.substr(world_prefix.size());
        if (value.empty()) {
            throw std::invalid_argument("--world requires a non-empty package path");
        }
        parsed.world_path = std::filesystem::path{std::string{value}};
    }
    return parsed;
}

TerrainVisualization build_terrain_visualization(
    const world_format::WorldPackage& package) {
    const world_format::TerrainTile& terrain = package.terrain;
    const std::size_t sample_count = validate_terrain_structure(terrain);

    TerrainVisualizationStatistics statistics{
        .source_columns = terrain.columns,
        .source_rows = terrain.rows,
        .display_columns = std::min(terrain.columns, maximum_preview_dimension),
        .display_rows = std::min(terrain.rows, maximum_preview_dimension),
    };

    for (std::size_t index = 0; index < sample_count; ++index) {
        if (terrain.validity[index] == 0U) {
            ++statistics.invalid_samples;
            continue;
        }
        ++statistics.valid_samples;
        const float height = terrain.heights[index];
        if (!statistics.minimum_valid_height.has_value()) {
            statistics.minimum_valid_height = height;
            statistics.maximum_valid_height = height;
        } else {
            statistics.minimum_valid_height =
                std::min(statistics.minimum_valid_height.value(), height);
            statistics.maximum_valid_height =
                std::max(statistics.maximum_valid_height.value(), height);
        }
    }

    render::DiagnosticRaster raster{
        .columns = statistics.display_columns,
        .rows = statistics.display_rows,
        .display_aspect_ratio = display_aspect_ratio(terrain),
        .linear_colours = {},
    };
    raster.linear_colours.reserve(
        static_cast<std::size_t>(statistics.display_columns)
        * static_cast<std::size_t>(statistics.display_rows));

    for (std::uint32_t display_row = 0U;
         display_row < statistics.display_rows;
         ++display_row) {
        const std::uint32_t source_row_begin = bin_boundary(
            display_row, terrain.rows, statistics.display_rows);
        const std::uint32_t source_row_end = bin_boundary(
            display_row + 1U, terrain.rows, statistics.display_rows);

        for (std::uint32_t display_column = 0U;
             display_column < statistics.display_columns;
             ++display_column) {
            const std::uint32_t source_column_begin = bin_boundary(
                display_column, terrain.columns, statistics.display_columns);
            const std::uint32_t source_column_end = bin_boundary(
                display_column + 1U, terrain.columns, statistics.display_columns);

            double valid_height_sum{};
            std::uint64_t valid_count{};
            for (std::uint32_t source_row = source_row_begin;
                 source_row < source_row_end;
                 ++source_row) {
                for (std::uint32_t source_column = source_column_begin;
                     source_column < source_column_end;
                     ++source_column) {
                    const std::size_t source_index =
                        static_cast<std::size_t>(source_row)
                            * static_cast<std::size_t>(terrain.columns)
                        + static_cast<std::size_t>(source_column);
                    if (terrain.validity[source_index] != 0U) {
                        valid_height_sum += static_cast<double>(terrain.heights[source_index]);
                        ++valid_count;
                    }
                }
            }

            const std::uint64_t bin_sample_count =
                static_cast<std::uint64_t>(source_row_end - source_row_begin)
                * static_cast<std::uint64_t>(source_column_end - source_column_begin);
            if (valid_count == 0U) {
                raster.linear_colours.push_back(invalid_colour);
                continue;
            }

            float normalized = 0.5F;
            const float minimum = statistics.minimum_valid_height.value();
            const float maximum = statistics.maximum_valid_height.value();
            if (minimum != maximum) {
                const double mean = valid_height_sum / static_cast<double>(valid_count);
                const double range = static_cast<double>(maximum)
                    - static_cast<double>(minimum);
                normalized = static_cast<float>(std::clamp(
                    (mean - static_cast<double>(minimum)) / range, 0.0, 1.0));
            }

            const float valid_fraction = static_cast<float>(valid_count)
                / static_cast<float>(bin_sample_count);
            raster.linear_colours.push_back(
                blend_validity(height_colour(normalized), valid_fraction));
        }
    }

    render::RenderFrame render_frame{
        .background = render::foundation_diagnostic_frame(),
        .raster = std::move(raster),
    };
    render::validate_render_frame(render_frame);
    return {
        .render_frame = std::move(render_frame),
        .statistics = statistics,
    };
}

void write_world_visualization_summary(
    std::ostream& output,
    const std::filesystem::path& path,
    const world_format::WorldPackage& package,
    const TerrainVisualization& visualization) {
    const world_format::TerrainTile& terrain = package.terrain;
    const world_format::Provenance& provenance = package.provenance;
    const TerrainVisualizationStatistics& statistics = visualization.statistics;

    std::ostringstream summary;
    summary << std::setprecision(17)
            << "World package loaded: path=" << std::quoted(path.generic_string())
            << " format=" << package.header.major_version << '.'
            << package.header.minor_version
            << " checksum_crc32=" << checksum_text(package.checksum) << '\n'
            << "World terrain: layer=" << std::quoted(terrain.layer_id)
            << " tile=" << std::quoted(terrain.tile_id)
            << " source=" << statistics.source_columns << 'x'
            << statistics.source_rows
            << " display=" << statistics.display_columns << 'x'
            << statistics.display_rows
            << " valid=" << statistics.valid_samples
            << " invalid=" << statistics.invalid_samples << '\n'
            << "World spatial: crs=EPSG:" << package.crs_epsg
            << " origin_x=" << terrain.origin_x
            << " origin_y=" << terrain.origin_y
            << " origin_z=" << terrain.origin_z
            << " spacing_x=" << terrain.spacing_x
            << " spacing_y=" << terrain.spacing_y
            << " display_aspect="
            << visualization.render_frame.raster.value().display_aspect_ratio << '\n';

    if (statistics.minimum_valid_height.has_value()) {
        summary << "World valid range: local_min="
                << statistics.minimum_valid_height.value()
                << " local_max=" << statistics.maximum_valid_height.value()
                << " absolute_min="
                << terrain.origin_z
                    + static_cast<double>(statistics.minimum_valid_height.value())
                << " absolute_max="
                << terrain.origin_z
                    + static_cast<double>(statistics.maximum_valid_height.value())
                << '\n';
    } else {
        summary << "World valid range: none (all source samples invalid)\n";
    }

    summary << "World provenance: provider=" << std::quoted(provenance.provider)
            << " product=" << std::quoted(provenance.product)
            << " edition=" << std::quoted(provenance.edition)
            << " source_classification="
            << std::quoted(provenance.source_classification)
            << " stage_classification="
            << std::quoted(provenance.stage_classification) << '\n'
            << "World attribution: " << std::quoted(provenance.attribution) << '\n'
            << "World visualization: mapping=linear_blue_green_yellow_global_valid_range;"
               "invalid=vivid_magenta;flat_valid=midpoint;"
               "aggregation=independent_floor_partition_valid_mean_with_invalid_fraction_blend;"
               "row_order=row0_lower_positive_y_upward\n";
    output << summary.str();
}

} // namespace game_ex::game::world_editor_detail
