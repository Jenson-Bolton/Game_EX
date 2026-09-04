/**
 * @file world_compiler_main.cpp
 * @brief Foundation entry point for the offline world compiler.
 */

#include "game_ex/world_format/world_header.hpp"

#include <iostream>

/**
 * @brief Reports the compiler scaffold without pretending to compile world data.
 * @return Zero because displaying foundation status is successful.
 *
 * Data ingestion, spatial reconciliation, TESSERA decoding, packaging, and CLI
 * design intentionally wait for their specifications.
 */
int main() {
    std::cout
        << "Game_EX World Compiler " << GAMEEX_VERSION_STRING
        << " foundation (world format "
        << game_ex::world_format::current_major_version << '.'
        << game_ex::world_format::current_minor_version << ")\n"
        << "No world compilation pipeline is configured yet.\n";
    return 0;
}
