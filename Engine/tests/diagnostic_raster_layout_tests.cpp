/**
 * @file diagnostic_raster_layout_tests.cpp
 * @brief Dependency-free tests for shared diagnostic raster pixel layout.
 */

#include "render/diagnostic_raster_layout.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

/**
 * @brief Reports one failed assertion without aborting the test executable.
 * @param condition Assertion result.
 * @param description Human-readable invariant.
 * @return True when the assertion passed.
 */
bool check(const bool condition, const std::string& description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
    }
    return condition;
}

/**
 * @brief Tests whether invoking a callable raises a selected exception type.
 * @tparam Exception Expected exception base or exact type.
 * @tparam Callable Nullary callable type.
 * @param callable Operation under test.
 * @return True only when the selected exception type was caught.
 */
template <typename Exception, typename Callable>
bool throws_exception(Callable&& callable) {
    try {
        std::invoke(std::forward<Callable>(callable));
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

/**
 * @brief Creates a valid raster with one repeated opaque colour.
 * @param columns Logical raster columns.
 * @param rows Logical raster rows.
 * @param aspect Intended display width divided by height.
 * @return Valid owning raster suitable for layout tests.
 */
[[nodiscard]] game_ex::render::DiagnosticRaster make_raster(
    const std::uint32_t columns,
    const std::uint32_t rows,
    const float aspect) {
    return {
        .columns = columns,
        .rows = rows,
        .display_aspect_ratio = aspect,
        .linear_colours = std::vector<game_ex::render::DiagnosticFrame>(
            static_cast<std::size_t>(columns) * rows,
            {0.25F, 0.5F, 0.75F, 1.0F}),
    };
}

/**
 * @brief Verifies letterboxing and the lower-edge row-zero convention.
 * @return True when a wide raster is centred and partitioned exactly.
 */
bool test_letterboxed_row_major_layout() {
    const auto raster = make_raster(4U, 2U, 2.0F);
    const auto layout = game_ex::render::detail::layout_diagnostic_raster(
        raster, 100U, 100U);

    bool passed = true;
    passed &= check(
        layout.x == 0U && layout.y == 25U
            && layout.width == 100U && layout.height == 50U,
        "2:1 content is vertically centred in a square drawable");
    passed &= check(layout.cells.size() == 8U, "layout retains one rectangle per cell");

    const auto& lower_left = layout.cells[0];
    passed &= check(
        lower_left.x == 0U && lower_left.y == 25U
            && lower_left.width == 25U && lower_left.height == 25U
            && lower_left.colour_index == 0U,
        "row zero begins at the lower content edge");

    const auto& upper_left = layout.cells[4];
    passed &= check(
        upper_left.x == 0U && upper_left.y == 50U
            && upper_left.width == 25U && upper_left.height == 25U
            && upper_left.colour_index == 4U,
        "second row follows row-major colour order above row zero");
    return passed;
}

/**
 * @brief Verifies deterministic floor partitions and asymmetric spare pixels.
 * @return True when odd extents neither overlap nor escape their content bounds.
 */
bool test_odd_integer_partitions() {
    const auto raster = make_raster(3U, 1U, 1.0F);
    const auto layout = game_ex::render::detail::layout_diagnostic_raster(
        raster, 101U, 50U);

    bool passed = check(
        layout.x == 25U && layout.y == 0U
            && layout.width == 50U && layout.height == 50U,
        "odd horizontal remainder places its spare pixel at the right edge");
    passed &= check(
        layout.cells[0].x == 25U && layout.cells[0].width == 16U,
        "first third uses the lower floor partition");
    passed &= check(
        layout.cells[1].x == 41U && layout.cells[1].width == 17U,
        "middle third begins at the previous exact boundary");
    passed &= check(
        layout.cells[2].x == 58U && layout.cells[2].width == 17U,
        "last third ends exactly at the content right edge");
    return passed;
}

/**
 * @brief Verifies tiny and maximum-size layouts retain exact safe partitions.
 * @return True when zero-area cells and 64-bit products remain deterministic.
 */
bool test_extreme_safe_partitions() {
    const auto raster = make_raster(64U, 64U, 1.0F);
    const auto tiny = game_ex::render::detail::layout_diagnostic_raster(
        raster, 1U, 1U);

    bool passed = check(
        tiny.cells.size() == game_ex::render::maximum_diagnostic_raster_cells,
        "tiny drawable still maps every logical colour cell");
    passed &= check(
        tiny.cells.front().width == 0U && tiny.cells.front().height == 0U,
        "cells may have zero pixel area when logical resolution is higher");
    passed &= check(
        tiny.cells.back().x == 0U && tiny.cells.back().y == 0U
            && tiny.cells.back().width == 1U && tiny.cells.back().height == 1U,
        "final tiny cell closes both floor partitions exactly");

    constexpr std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max();
    const auto huge = game_ex::render::detail::layout_diagnostic_raster(
        raster, maximum, maximum);
    const auto& last = huge.cells.back();
    passed &= check(
        last.x + last.width == maximum && last.y + last.height == maximum,
        "64-bit partition products preserve maximum drawable endpoints");
    return passed;
}

/**
 * @brief Verifies the private layout boundary rejects invalid input explicitly.
 * @return True when invalid rasters and zero extents cannot reach partitioning.
 */
bool test_invalid_layout_input() {
    const auto raster = make_raster(1U, 1U, 1.0F);
    bool passed = true;
    passed &= check(
        throws_exception<std::invalid_argument>([&raster] {
            static_cast<void>(game_ex::render::detail::layout_diagnostic_raster(
                raster, 0U, 1U));
        }),
        "zero drawable width is rejected");
    passed &= check(
        throws_exception<std::invalid_argument>([&raster] {
            static_cast<void>(game_ex::render::detail::layout_diagnostic_raster(
                raster, 1U, 0U));
        }),
        "zero drawable height is rejected");

    auto invalid = raster;
    invalid.linear_colours.clear();
    passed &= check(
        throws_exception<std::invalid_argument>([&invalid] {
            static_cast<void>(game_ex::render::detail::layout_diagnostic_raster(
                invalid, 10U, 10U));
        }),
        "invalid raster storage is rejected before partitioning");
    return passed;
}

} // namespace

/**
 * @brief Runs every shared diagnostic raster layout test.
 * @return EXIT_SUCCESS when all layout invariants hold.
 */
int main() {
    bool passed = true;
    passed &= test_letterboxed_row_major_layout();
    passed &= test_odd_integer_partitions();
    passed &= test_extreme_safe_partitions();
    passed &= test_invalid_layout_input();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
