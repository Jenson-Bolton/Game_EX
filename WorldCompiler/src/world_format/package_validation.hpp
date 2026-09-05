/**
 * @file package_validation.hpp
 * @brief Internal semantic validation required by runtime decoding.
 */

#pragma once

#include "game_ex/world_format/world_package.hpp"

namespace game_ex::world_format::detail {

/** Canonical stage axis order required by WorldFormat 0.2. */
inline constexpr auto required_stage_axis_order = "X,Y";

/** Canonical stage horizontal unit required by WorldFormat 0.2. */
inline constexpr auto required_stage_horizontal_units = "metre";

/** Canonical stage vertical unit required by WorldFormat 0.2. */
inline constexpr auto required_stage_vertical_units = "metre";

/** Canonical stage no-data representation required by WorldFormat 0.2. */
inline constexpr auto required_stage_no_data = "validity_mask";

/**
 * @brief Enforces WorldFormat 0.2's fixed normalized-stage semantics.
 * @param provenance Decoded provenance values.
 * @throws PackageError with `invalid_format` when any fixed value differs.
 */
void validate_fixed_stage_semantics(const Provenance& provenance);

} // namespace game_ex::world_format::detail
