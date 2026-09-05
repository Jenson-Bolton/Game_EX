/**
 * @file world_format_runtime_tests.cpp
 * @brief Runtime-only link and canonical golden package decoding test.
 */

#include "game_ex/world_format/world_package.hpp"
#include "hex_fixture.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/**
 * @brief Requires one runtime-fixture invariant.
 * @param condition Condition that must hold.
 * @param message Failure diagnostic.
 */
void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/**
 * @brief Writes exact fixture bytes to a test-owned package path.
 * @param path Destination package path.
 * @param bytes Canonical package bytes.
 */
void write_bytes(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!bytes.empty()) {
        stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error("Cannot write runtime golden package: " + path.string());
    }
}

} // namespace

/**
 * @brief Loads the checked-in golden while linking only GameEX::WorldFormat.
 * @return Zero on success and one on any failed invariant.
 */
int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path package_path = std::filesystem::temp_directory_path()
        / ("gameex_world_format_runtime_" + std::to_string(stamp) + ".gexworld");
    try {
        const std::vector<std::uint8_t> bytes =
            game_ex::world_test_detail::read_hex_fixture(GAMEEX_WORLD_GOLDEN_HEX);
        expect(bytes.size() == 1'151U, "Golden package must decode to exactly 1,151 bytes");
        write_bytes(package_path, bytes);

        const game_ex::world_format::WorldPackage package =
            game_ex::world_format::read_world_package(package_path);
        expect(package.header.magic == game_ex::world_format::world_file_magic,
            "Golden package magic is incorrect");
        expect(package.header.major_version == 0U && package.header.minor_version == 2U,
            "Golden package is not WorldFormat 0.2");
        expect(package.crs_epsg == 5'514U, "Golden package is not EPSG:5514");
        expect(package.terrain.columns == 2U && package.terrain.rows == 2U,
            "Golden package dimensions are incorrect");
        expect(package.terrain.origin_x == -742000.125
                && package.terrain.origin_y == -1045000.5
                && package.terrain.origin_z == 250.25,
            "Golden package origins are incorrect");
        expect(package.terrain.heights == std::vector<float>({0.0F, 1.5F, -2.0F, 4.0F}),
            "Golden package local heights are incorrect");
        expect(package.terrain.validity == std::vector<std::uint8_t>({1U, 1U, 0U, 1U}),
            "Golden package validity mask is incorrect");
        expect(package.provenance.stage_axis_order == "X,Y"
                && package.provenance.stage_horizontal_units == "metre"
                && package.provenance.stage_vertical_units == "metre"
                && package.provenance.stage_no_data_convention == "validity_mask",
            "Golden package normalized-stage semantics are incorrect");
        expect(package.checksum == 0x9AAE2360U, "Golden package CRC-32 is incorrect");

        std::error_code cleanup_error;
        static_cast<void>(std::filesystem::remove(package_path, cleanup_error));
        if (cleanup_error) {
            throw std::runtime_error(
                "Cannot remove runtime golden package: " + cleanup_error.message());
        }
        std::cout << "WorldFormat runtime-only golden test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        static_cast<void>(std::filesystem::remove(package_path, cleanup_error));
        std::cerr << "WorldFormat runtime-only golden test failed: " << error.what() << '\n';
        return 1;
    }
}
