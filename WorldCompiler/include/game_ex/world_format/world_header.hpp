/**
 * @file world_header.hpp
 * @brief Versioned identity header for Game_EX runtime world packages.
 */

#pragma once

#include <array>
#include <cstdint>

namespace game_ex::world_format {

/**
 * @brief Current major world-format version understood by this source tree.
 *
 * A major-version change denotes an incompatible format change.
 *
 * @ingroup world_format
 */
inline constexpr std::uint16_t current_major_version = 0;

/**
 * @brief Current minor world-format version understood by this source tree.
 *
 * Version 0.2 is the first concrete serialized package contract. Future minor
 * revisions may be backward-compatible, but a decoder must implement each
 * accepted layout explicitly rather than assuming layout compatibility.
 *
 * @ingroup world_format
 */
inline constexpr std::uint16_t current_minor_version = 2;

/**
 * @brief File signature used to reject unrelated or corrupt inputs early.
 * @ingroup world_format
 */
inline constexpr std::array<char, 8> world_file_magic{
    'G', 'A', 'M', 'E', 'E', 'X', 'W', 'D'};

/**
 * @brief Logical header values shared by the compiler and runtime reader.
 *
 * This structure is a semantic model and is never dumped directly to disk. The
 * `.gexworld` reader decodes every field from the documented little-endian wire
 * representation.
 *
 * @ingroup world_format
 */
struct WorldHeader final {
    /** Signature identifying a Game_EX world package. */
    std::array<char, 8> magic{world_file_magic};

    /** Incompatible-format version component. */
    std::uint16_t major_version{current_major_version};

    /** Backward-compatible-format version component. */
    std::uint16_t minor_version{current_minor_version};
};

/**
 * @brief Determines whether this source tree can read a logical world header.
 * @param header Header values obtained through the package decoder.
 * @return True when the signature and major version are supported.
 *
 * This lightweight identity check accepts the current major version. Concrete
 * package decoding additionally requires the exact first serialized minor
 * version because no earlier on-disk package contract existed.
 *
 * @ingroup world_format
 */
[[nodiscard]] bool is_supported(const WorldHeader& header) noexcept;

} // namespace game_ex::world_format
