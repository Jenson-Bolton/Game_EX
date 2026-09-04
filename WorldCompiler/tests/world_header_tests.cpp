/**
 * @file world_header_tests.cpp
 * @brief Dependency-free checks for world-header compatibility rules.
 */

#include "game_ex/world_format/world_header.hpp"

#include <cstdlib>

/**
 * @brief Verifies accepted defaults and rejection of incompatible headers.
 * @return EXIT_SUCCESS when every compatibility invariant holds.
 */
int main() {
    using game_ex::world_format::WorldHeader;
    using game_ex::world_format::is_supported;

    const WorldHeader supported{};
    if (!is_supported(supported)) {
        return EXIT_FAILURE;
    }

    WorldHeader wrong_magic{};
    wrong_magic.magic.front() = 'X';
    if (is_supported(wrong_magic)) {
        return EXIT_FAILURE;
    }

    WorldHeader wrong_major{};
    ++wrong_major.major_version;
    if (is_supported(wrong_major)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
