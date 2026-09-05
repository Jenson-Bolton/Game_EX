/**
 * @file package_encoding.hpp
 * @brief Offline-only canonical `.gexworld` encoder declaration.
 */

#pragma once

#include "game_ex/world_format/world_package.hpp"

#include <cstdint>
#include <vector>

namespace game_ex::world_format::detail {

/**
 * @brief Deterministically encodes one fully specified logical package.
 * @param package Logical one-layer/one-tile package.
 * @return Canonical little-endian bytes including computed CRC-32.
 * @throws PackageError if logical values violate package bounds or fixed stage semantics.
 * @throws std::bad_alloc if bounded output allocation cannot be satisfied.
 */
[[nodiscard]] std::vector<std::uint8_t> encode_world_package(
    const WorldPackage& package);

} // namespace game_ex::world_format::detail
