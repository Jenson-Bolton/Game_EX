/**
 * @file world_header.cpp
 * @brief World-package compatibility checks.
 */

#include "game_ex/world_format/world_header.hpp"

namespace game_ex::world_format {

bool is_supported(const WorldHeader& header) noexcept {
    return header.magic == world_file_magic
        && header.major_version == current_major_version;
}

} // namespace game_ex::world_format
