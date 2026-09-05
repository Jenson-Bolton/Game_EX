/**
 * @file world_editor_model.hpp
 * @brief Pure command-line and terrain-raster preparation for the world editor.
 */

#pragma once

#include "game_ex/render/renderer.hpp"
#include "game_ex/world_format/world_package.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace game_ex::game::world_editor_detail {

/** Maximum independently downsampled world-preview dimension. */
inline constexpr std::uint32_t maximum_preview_dimension =
    render::maximum_diagnostic_raster_dimension;

/**
 * @brief Editor-specific command line separated from shared desktop options.
 */
struct WorldEditorCommandLine final {
    /** Optional package that must be loaded before desktop composition. */
    std::optional<std::filesystem::path> world_path;

    /** Non-editor arguments preserved in their original order. */
    std::vector<std::string_view> desktop_arguments;
};

/**
 * @brief Parses and removes the editor's one optional world-package argument.
 * @param arguments Process arguments after the executable name.
 * @return Optional world path and untouched shared desktop arguments.
 * @throws std::invalid_argument if `--world=` is empty or supplied more than once.
 */
[[nodiscard]] WorldEditorCommandLine parse_world_editor_command_line(
    std::span<const std::string_view> arguments);

/**
 * @brief Observable source and aggregation facts for one terrain preview.
 */
struct TerrainVisualizationStatistics final {
    /** Source terrain columns before independent downsampling. */
    std::uint32_t source_columns{};

    /** Source terrain rows before independent downsampling. */
    std::uint32_t source_rows{};

    /** Display columns after applying the 64-cell bound. */
    std::uint32_t display_columns{};

    /** Display rows after applying the 64-cell bound. */
    std::uint32_t display_rows{};

    /** Number of valid source samples used for global normalization. */
    std::uint64_t valid_samples{};

    /** Number of invalid source samples. */
    std::uint64_t invalid_samples{};

    /** Lowest valid tile-local height, absent when all samples are invalid. */
    std::optional<float> minimum_valid_height;

    /** Highest valid tile-local height, absent when all samples are invalid. */
    std::optional<float> maximum_valid_height;
};

/**
 * @brief Owning backend input plus the facts used to explain its construction.
 */
struct TerrainVisualization final {
    /** Background and bounded row-major diagnostic colour raster. */
    render::RenderFrame render_frame;

    /** Source validity, range, and display-dimension facts. */
    TerrainVisualizationStatistics statistics;
};

/**
 * @brief Converts one validated world terrain tile into a diagnostic raster.
 * @param package Runtime package whose sole terrain layer is visualized.
 * @return Owning render frame and deterministic aggregation statistics.
 * @throws std::invalid_argument if the supplied in-memory tile is inconsistent.
 *
 * Source rows are already ordered from the lower `origin_y` edge toward +Y, so
 * row zero remains the lower display row. Each axis is independently bounded to
 * 64 cells. Floor-division bin boundaries partition every source sample exactly
 * once. Valid values are averaged per bin and normalized with the global valid
 * source range. The linear blue-green-yellow ramp is blended toward vivid
 * magenta by each bin's invalid fraction; zero-valid bins are entirely magenta.
 * A globally flat valid tile maps to the ramp midpoint. Display aspect uses a
 * half-cell-padded sample footprint (`columns * spacing_x` by
 * `rows * spacing_y`), distinct from the package's sample-centre bounds.
 */
[[nodiscard]] TerrainVisualization build_terrain_visualization(
    const world_format::WorldPackage& package);

/**
 * @brief Writes the complete reproducibility summary for a loaded world preview.
 * @param output Destination diagnostic stream.
 * @param path Package path supplied by the user.
 * @param package Verified decoded package.
 * @param visualization Exact frame-building statistics.
 */
void write_world_visualization_summary(
    std::ostream& output,
    const std::filesystem::path& path,
    const world_format::WorldPackage& package,
    const TerrainVisualization& visualization);

} // namespace game_ex::game::world_editor_detail
