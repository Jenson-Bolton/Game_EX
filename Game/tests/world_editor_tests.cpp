/**
 * @file world_editor_tests.cpp
 * @brief Exact tests for world-editor parsing and terrain visualization.
 */

#include "world_editor_model.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using game_ex::game::world_editor_detail::TerrainVisualization;
using game_ex::render::DiagnosticFrame;
using game_ex::world_format::WorldPackage;

/** Accepted absolute error for deterministic float colour comparisons. */
constexpr float colour_epsilon = 0.00001F;

/**
 * @brief Reports one failed assertion without skipping subsequent cases.
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
 * @brief Compares one colour component within the test tolerance.
 * @param actual Observed component.
 * @param expected Required component.
 * @return True when the component is sufficiently close.
 */
[[nodiscard]] bool close(const float actual, const float expected) noexcept {
    return std::fabs(actual - expected) <= colour_epsilon;
}

/**
 * @brief Compares all four linear colour components.
 * @param actual Observed colour.
 * @param expected Required colour.
 * @return True when every component is sufficiently close.
 */
[[nodiscard]] bool same_colour(
    const DiagnosticFrame& actual,
    const DiagnosticFrame& expected) noexcept {
    return close(actual.red, expected.red)
        && close(actual.green, expected.green)
        && close(actual.blue, expected.blue)
        && close(actual.alpha, expected.alpha);
}

/**
 * @brief Builds a structurally valid in-memory package for pure visualization tests.
 * @param columns Source columns.
 * @param rows Source rows.
 * @param heights Exact row-major tile-local heights.
 * @param validity Exact row-major validity bytes.
 * @param spacing_x Physical column spacing.
 * @param spacing_y Physical row spacing.
 * @return Package model accepted by the visualization boundary.
 */
[[nodiscard]] WorldPackage make_package(
    const std::uint32_t columns,
    const std::uint32_t rows,
    std::vector<float> heights,
    std::vector<std::uint8_t> validity,
    const double spacing_x = 2.0,
    const double spacing_y = 2.0) {
    WorldPackage package{};
    package.terrain.layer_id = "test_layer";
    package.terrain.tile_id = "test_tile";
    package.terrain.columns = columns;
    package.terrain.rows = rows;
    package.terrain.origin_x = -742000.0;
    package.terrain.origin_y = -1045000.0;
    package.terrain.origin_z = 250.0;
    package.terrain.spacing_x = spacing_x;
    package.terrain.spacing_y = spacing_y;
    package.terrain.heights = std::move(heights);
    package.terrain.validity = std::move(validity);
    package.provenance.provider = "test provider";
    package.provenance.product = "test product";
    package.provenance.edition = "test edition";
    package.provenance.attribution = "test attribution";
    package.provenance.source_classification = "test source class";
    package.provenance.stage_classification = "test stage class";
    package.checksum = 0x0123ABCDU;
    return package;
}

/**
 * @brief Returns whether editor parsing rejects selected input.
 * @param arguments Process arguments after the executable name.
 * @return True only when std::invalid_argument is raised.
 */
[[nodiscard]] bool editor_parse_is_rejected(
    const std::span<const std::string_view> arguments) {
    try {
        static_cast<void>(
            game_ex::game::world_editor_detail::parse_world_editor_command_line(arguments));
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

/**
 * @brief Verifies exact one-time world parsing and common-option preservation.
 * @return True when the editor grammar meets its public contract.
 */
bool test_editor_command_line() {
    const std::array<std::string_view, 0U> none{};
    const auto defaults =
        game_ex::game::world_editor_detail::parse_world_editor_command_line(none);

    const std::array mixed{
        std::string_view{"--renderer=vulkan"},
        std::string_view{"--world=data/test.gexworld"},
        std::string_view{"--quit-after-ms=250"},
    };
    const auto parsed =
        game_ex::game::world_editor_detail::parse_world_editor_command_line(mixed);

    const std::array near_match{std::string_view{"--worldish=not-an-editor-option"}};
    const auto forwarded =
        game_ex::game::world_editor_detail::parse_world_editor_command_line(near_match);

    bool passed = check(!defaults.world_path.has_value()
                            && defaults.desktop_arguments.empty(),
                        "world omission preserves the no-world shell");
    passed &= check(parsed.world_path.has_value()
                        && parsed.world_path->generic_string() == "data/test.gexworld",
                    "one non-empty world path parses exactly");
    passed &= check(parsed.desktop_arguments
                            == std::vector<std::string_view>{
                                "--renderer=vulkan", "--quit-after-ms=250"},
                    "world option is removed and common option order is preserved");
    passed &= check(!forwarded.world_path.has_value()
                        && forwarded.desktop_arguments
                            == std::vector<std::string_view>{
                                "--worldish=not-an-editor-option"},
                    "near-match is left for strict common parsing");

    const std::array empty{std::string_view{"--world="}};
    const std::array repeated{
        std::string_view{"--world=a.gexworld"},
        std::string_view{"--world=b.gexworld"},
    };
    passed &= check(editor_parse_is_rejected(empty), "empty world path is rejected");
    passed &= check(editor_parse_is_rejected(repeated), "repeated world path is rejected");
    return passed;
}

/**
 * @brief Verifies the exact blue-green-yellow ramp and lower-row ordering.
 * @return True when global mapping and row-major orientation are deterministic.
 */
bool test_height_mapping_and_orientation() {
    const WorldPackage ramp_package = make_package(
        5U,
        1U,
        {0.0F, 1.0F, 2.0F, 3.0F, 4.0F},
        {1U, 1U, 1U, 1U, 1U});
    const TerrainVisualization ramp =
        game_ex::game::world_editor_detail::build_terrain_visualization(ramp_package);
    const auto& colours = ramp.render_frame.raster->linear_colours;
    const std::array expected{
        DiagnosticFrame{0.0F, 0.0F, 1.0F, 1.0F},
        DiagnosticFrame{0.0F, 0.5F, 0.5F, 1.0F},
        DiagnosticFrame{0.0F, 1.0F, 0.0F, 1.0F},
        DiagnosticFrame{0.5F, 1.0F, 0.0F, 1.0F},
        DiagnosticFrame{1.0F, 1.0F, 0.0F, 1.0F},
    };

    bool passed = check(colours.size() == expected.size(),
                        "unscaled ramp retains every source cell");
    for (std::size_t index = 0; index < expected.size() && index < colours.size(); ++index) {
        passed &= check(same_colour(colours[index], expected[index]),
                        "ramp cell has the exact linear colour");
    }

    const WorldPackage rows = make_package(1U, 2U, {0.0F, 4.0F}, {1U, 1U});
    const TerrainVisualization oriented =
        game_ex::game::world_editor_detail::build_terrain_visualization(rows);
    passed &= check(same_colour(
                         oriented.render_frame.raster->linear_colours[0],
                         DiagnosticFrame{0.0F, 0.0F, 1.0F, 1.0F})
                        && same_colour(
                            oriented.render_frame.raster->linear_colours[1],
                            DiagnosticFrame{1.0F, 1.0F, 0.0F, 1.0F}),
                    "source row zero remains the lower raster row");
    return passed;
}

/**
 * @brief Verifies invalid, partially valid, and globally flat colour rules.
 * @return True when validity blending is exact.
 */
bool test_validity_and_flat_mapping() {
    std::vector<float> heights(65U, 7.0F);
    std::vector<std::uint8_t> validity(65U, 1U);
    validity.back() = 0U;
    const TerrainVisualization partial =
        game_ex::game::world_editor_detail::build_terrain_visualization(
            make_package(65U, 1U, std::move(heights), std::move(validity)));
    const auto& partial_colours = partial.render_frame.raster->linear_colours;

    bool passed = check(partial.render_frame.raster->columns == 64U,
                        "65 columns are bounded to 64 floor-partition bins");
    passed &= check(same_colour(
                         partial_colours.front(),
                         DiagnosticFrame{0.0F, 1.0F, 0.0F, 1.0F}),
                    "flat valid bins use the green ramp midpoint");
    passed &= check(same_colour(
                         partial_colours.back(),
                         DiagnosticFrame{0.5F, 0.5F, 0.5F, 1.0F}),
                    "the two-sample final bin blends half valid green with magenta");

    const TerrainVisualization invalid =
        game_ex::game::world_editor_detail::build_terrain_visualization(
            make_package(1U, 1U, {123.0F}, {0U}));
    passed &= check(same_colour(
                         invalid.render_frame.raster->linear_colours.front(),
                         DiagnosticFrame{1.0F, 0.0F, 1.0F, 1.0F})
                        && !invalid.statistics.minimum_valid_height.has_value()
                        && !invalid.statistics.maximum_valid_height.has_value(),
                    "a zero-valid bin is vivid magenta with no invented range");
    return passed;
}

/**
 * @brief Verifies the exact real-preview-sized independent downsample bound.
 * @return True when 251x201 deterministically becomes 64x64.
 */
bool test_large_downsample_dimensions() {
    constexpr std::uint32_t columns = 251U;
    constexpr std::uint32_t rows = 201U;
    constexpr std::size_t sample_count =
        static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    const TerrainVisualization visualization =
        game_ex::game::world_editor_detail::build_terrain_visualization(
            make_package(
                columns,
                rows,
                std::vector<float>(sample_count, 5.0F),
                std::vector<std::uint8_t>(sample_count, 1U),
                10.0,
                5.0));
    const auto& raster = visualization.render_frame.raster.value();
    const float expected_aspect = static_cast<float>(
        (static_cast<double>(columns) * 10.0)
        / (static_cast<double>(rows) * 5.0));

    bool passed = check(raster.columns == 64U && raster.rows == 64U,
                        "251x201 independently downsamples to exactly 64x64");
    passed &= check(raster.linear_colours.size() == 4096U,
                    "64x64 output owns exactly 4096 colours");
    passed &= check(visualization.statistics.valid_samples == sample_count
                        && visualization.statistics.invalid_samples == 0U,
                    "all 251x201 source samples participate in global statistics");
    passed &= check(close(raster.display_aspect_ratio, expected_aspect),
                    "display aspect uses the half-cell-padded sample footprint");
    passed &= check(same_colour(
                         raster.linear_colours.front(),
                         DiagnosticFrame{0.0F, 1.0F, 0.0F, 1.0F})
                        && same_colour(
                            raster.linear_colours.back(),
                            DiagnosticFrame{0.0F, 1.0F, 0.0F, 1.0F}),
                    "complete flat source partitions remain midpoint green");
    return passed;
}

/**
 * @brief Verifies the loaded-package summary contains required provenance facts.
 * @return True when the stable summary is complete.
 */
bool test_summary() {
    const WorldPackage package = make_package(2U, 1U, {0.0F, 2.0F}, {1U, 0U});
    const TerrainVisualization visualization =
        game_ex::game::world_editor_detail::build_terrain_visualization(package);
    std::ostringstream output;
    game_ex::game::world_editor_detail::write_world_visualization_summary(
        output, "fixture.gexworld", package, visualization);
    const std::string text = output.str();

    bool passed = check(text.find("checksum_crc32=0123abcd") != std::string::npos,
                        "summary reports the verified checksum");
    passed &= check(text.find("layer=\"test_layer\" tile=\"test_tile\"")
                            != std::string::npos,
                        "summary reports layer and tile identity");
    passed &= check(text.find("source=2x1 display=2x1 valid=1 invalid=1")
                            != std::string::npos,
                        "summary reports dimensions and validity counts");
    passed &= check(text.find("crs=EPSG:5514") != std::string::npos
                        && text.find("spacing_x=2") != std::string::npos,
                    "summary reports CRS and spacing");
    passed &= check(text.find("provider=\"test provider\"") != std::string::npos
                        && text.find("edition=\"test edition\"") != std::string::npos
                        && text.find("source_classification=\"test source class\"")
                            != std::string::npos
                        && text.find("World attribution: \"test attribution\"")
                            != std::string::npos,
                    "summary reports classifications, provider, edition, and attribution");
    passed &= check(text.find("aggregation=independent_floor_partition_valid_mean")
                            != std::string::npos
                        && text.find("row_order=row0_lower_positive_y_upward")
                            != std::string::npos,
                    "summary documents mapping, aggregation, and row orientation");
    return passed;
}

/**
 * @brief Verifies direct callers cannot bypass in-memory structure validation.
 * @return True when invalid spatial metadata and storage are rejected.
 */
bool test_invalid_in_memory_structure() {
    WorldPackage invalid_origin = make_package(1U, 1U, {0.0F}, {1U});
    invalid_origin.terrain.origin_x = std::numeric_limits<double>::infinity();

    WorldPackage invalid_storage = make_package(2U, 1U, {0.0F}, {1U, 1U});

    bool rejected_origin = false;
    try {
        static_cast<void>(
            game_ex::game::world_editor_detail::build_terrain_visualization(invalid_origin));
    } catch (const std::invalid_argument&) {
        rejected_origin = true;
    }

    bool rejected_storage = false;
    try {
        static_cast<void>(
            game_ex::game::world_editor_detail::build_terrain_visualization(invalid_storage));
    } catch (const std::invalid_argument&) {
        rejected_storage = true;
    }

    bool passed = check(rejected_origin, "non-finite direct-call origin is rejected");
    passed &= check(rejected_storage, "inconsistent direct-call sample storage is rejected");
    return passed;
}

} // namespace

/**
 * @brief Runs all pure world-editor model tests.
 * @return EXIT_SUCCESS when every assertion passes.
 */
int main() {
    bool passed = true;
    passed &= test_editor_command_line();
    passed &= test_height_mapping_and_orientation();
    passed &= test_validity_and_flat_mapping();
    passed &= test_large_downsample_dimensions();
    passed &= test_summary();
    passed &= test_invalid_in_memory_structure();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
