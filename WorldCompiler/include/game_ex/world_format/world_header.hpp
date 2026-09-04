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
 * A minor-version change is reserved for backward-compatible additions.
 *
 * @ingroup world_format
 */
inline constexpr std::uint16_t current_minor_version = 1;

/**
 * @brief File signature used to reject unrelated or corrupt inputs early.
 * @ingroup world_format
 */
inline constexpr std::array<char, 8> world_file_magic{
    'G', 'A', 'M', 'E', 'E', 'X', 'W', 'D'};

/**
 * @brief Logical header values shared by the compiler and runtime reader.
 *
 * This structure is a semantic model, not a promise that its in-memory layout
 * will be written directly to disk. The serialization scheme remains a future
 * specification decision.
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
 * @param header Header values obtained through a future decoder.
 * @return True when the signature and major version are supported.
 *
 * Minor versions do not currently participate in rejection because the format
 * policy reserves them for backward-compatible additions.
 *
 * @ingroup world_format
 */
[[nodiscard]] bool is_supported(const WorldHeader& header) noexcept;

} // namespace game_ex::world_format
