/**
 * @file package_validation.cpp
 * @brief Runtime-required WorldFormat semantic validation.
 */

#include "world_format/package_validation.hpp"

#include <string>

namespace game_ex::world_format::detail {
namespace {

/**
 * @brief Throws the standard logical-format error.
 * @param message Human-readable failed invariant.
 */
[[noreturn]] void invalid(const std::string& message) {
    throw PackageError(PackageErrorCode::invalid_format, message);
}

/**
 * @brief Requires one exact normalized-stage string.
 * @param actual Value supplied by a decoded package.
 * @param expected Canonical format value.
 * @param field Human-readable field name.
 */
void require_exact(
    const std::string& actual,
    const char* const expected,
    const char* const field) {
    if (actual != expected) {
        invalid(std::string(field) + " must equal " + expected);
    }
}

} // namespace

void validate_fixed_stage_semantics(const Provenance& provenance) {
    require_exact(
        provenance.stage_axis_order, required_stage_axis_order, "Stage axis order");
    require_exact(
        provenance.stage_horizontal_units,
        required_stage_horizontal_units,
        "Stage horizontal units");
    require_exact(
        provenance.stage_vertical_units,
        required_stage_vertical_units,
        "Stage vertical units");
    require_exact(
        provenance.stage_no_data_convention,
        required_stage_no_data,
        "Stage no-data convention");
}

} // namespace game_ex::world_format::detail
