/**
 * @file world_compiler_main.cpp
 * @brief Stable command-line interface for staging validation and packaging.
 */

#include "game_ex/world_compiler/world_compiler.hpp"
#include "game_ex/world_format/world_package.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

/** Stable process exit categories used by scripts and CI. */
enum class ExitCode : int {
    /** The requested operation completed successfully. */
    success = 0,
    /** Command syntax or option spelling was invalid. */
    usage = 2,
    /** The staging manifest violated its schema. */
    invalid_manifest = 3,
    /** A manifest-referenced sample file was unsafe or invalid. */
    invalid_staged_input = 4,
    /** A package was malformed, unsupported, or corrupt. */
    invalid_package = 5,
    /** A required input or output operation failed. */
    io = 6,
    /** An unexpected implementation or allocation failure occurred. */
    unexpected = 7
};

/**
 * @brief Prints the canonical command reference.
 * @param stream Destination stream.
 */
void print_help(std::ostream& stream) {
    stream
        << "Game_EX World Compiler " << GAMEEX_VERSION_STRING << '\n'
        << "Usage:\n"
        << "  game_ex_world_compiler --help\n"
        << "  game_ex_world_compiler --version\n"
        << "  game_ex_world_compiler validate --manifest <path>\n"
        << "  game_ex_world_compiler compile --manifest <path> --output <path.gexworld>\n"
        << "  game_ex_world_compiler inspect --package <path>\n";
}

/**
 * @brief Counts valid samples without changing their stored order.
 * @param package Decoded package.
 * @return Number of mask bytes equal to one.
 */
[[nodiscard]] std::size_t valid_sample_count(
    const game_ex::world_format::WorldPackage& package) {
    return static_cast<std::size_t>(std::count(
        package.terrain.validity.begin(),
        package.terrain.validity.end(),
        static_cast<std::uint8_t>(1)));
}

/**
 * @brief Prints stable common metadata for validation or inspection.
 * @param package Validated logical package.
 */
void print_package_summary(const game_ex::world_format::WorldPackage& package) {
    const auto& terrain = package.terrain;
    const auto& provenance = package.provenance;
    std::cout
        << "world_format=" << package.header.major_version << '.'
        << package.header.minor_version << '\n'
        << "crs=EPSG:" << package.crs_epsg << '\n'
        << "terrain_layers=1\n"
        << "layer_id=" << terrain.layer_id << '\n'
        << "terrain_tiles=1\n"
        << "tile_id=" << terrain.tile_id << '\n'
        << "dimensions=" << terrain.columns << 'x' << terrain.rows << '\n'
        << "samples=" << terrain.heights.size() << '\n'
        << "valid_samples=" << valid_sample_count(package) << '\n'
        << std::setprecision(std::numeric_limits<double>::max_digits10)
        << "origin_x=" << terrain.origin_x << '\n'
        << "origin_y=" << terrain.origin_y << '\n'
        << "origin_z=" << terrain.origin_z << '\n'
        << "spacing_x=" << terrain.spacing_x << '\n'
        << "spacing_y=" << terrain.spacing_y << '\n'
        << "tile_min_x=" << terrain.origin_x << '\n'
        << "tile_max_x=" << terrain.origin_x
            + terrain.spacing_x * static_cast<double>(terrain.columns - 1U) << '\n'
        << "tile_min_y=" << terrain.origin_y << '\n'
        << "tile_max_y=" << terrain.origin_y
            + terrain.spacing_y * static_cast<double>(terrain.rows - 1U) << '\n'
        << "provenance.provider=" << provenance.provider << '\n'
        << "provenance.product=" << provenance.product << '\n'
        << "provenance.edition=" << provenance.edition << '\n'
        << "provenance.source_uri=" << provenance.source_uri << '\n'
        << "provenance.acquired_utc=" << provenance.acquisition_timestamp << '\n'
        << "provenance.licence_name=" << provenance.licence_name << '\n'
        << "provenance.licence_uri=" << provenance.licence_uri << '\n'
        << "provenance.attribution=" << provenance.attribution << '\n'
        << "provenance.rights=" << provenance.rights_statement << '\n'
        << "provenance.source_filename=" << provenance.source_filename << '\n'
        << "provenance.source_byte_size=" << provenance.source_byte_size << '\n'
        << "provenance.source_sha256=" << provenance.source_sha256 << '\n'
        << "provenance.source_classification=" << provenance.source_classification << '\n'
        << "provenance.stage_classification=" << provenance.stage_classification << '\n'
        << "source.crs=" << provenance.source_crs << '\n'
        << "source.axis_order=" << provenance.source_axis_order << '\n'
        << "source.horizontal_units=" << provenance.source_horizontal_units << '\n'
        << "source.vertical_reference=" << provenance.source_vertical_reference << '\n'
        << "source.vertical_units=" << provenance.source_vertical_units << '\n'
        << "source.no_data=" << provenance.source_no_data_convention << '\n'
        << "stage.axis_order=" << provenance.stage_axis_order << '\n'
        << "stage.horizontal_units=" << provenance.stage_horizontal_units << '\n'
        << "stage.vertical_reference=" << provenance.stage_vertical_reference << '\n'
        << "stage.vertical_units=" << provenance.stage_vertical_units << '\n'
        << "stage.no_data=" << provenance.stage_no_data_convention << '\n'
        << "terrain.resolution=" << provenance.resolution << '\n'
        << "spatial.bounds=" << provenance.bounds << '\n'
        << "spatial.epoch=" << provenance.epoch << '\n'
        << "provenance.transformation=" << provenance.transformation << '\n'
        << "provenance.responsible_owner=" << provenance.responsible_owner << '\n'
        << "staged.heights.path=" << provenance.staged_heights_path << '\n'
        << "staged.heights.byte_size=" << provenance.staged_heights_byte_size << '\n'
        << "staged.heights.sha256=" << provenance.staged_heights_sha256 << '\n'
        << "staged.validity.path=" << provenance.staged_validity_path << '\n'
        << "staged.validity.byte_size=" << provenance.staged_validity_byte_size << '\n'
        << "staged.validity.sha256=" << provenance.staged_validity_sha256 << '\n';

    bool found_valid = false;
    float minimum = 0.0F;
    float maximum = 0.0F;
    for (std::size_t index = 0; index < terrain.heights.size(); ++index) {
        if (terrain.validity[index] == 0U) {
            continue;
        }
        const float height = terrain.heights[index];
        if (!found_valid) {
            minimum = height;
            maximum = height;
            found_valid = true;
        } else {
            minimum = std::min(minimum, height);
            maximum = std::max(maximum, height);
        }
    }
    if (found_valid) {
        const double absolute_minimum = terrain.origin_z + static_cast<double>(minimum);
        const double absolute_maximum = terrain.origin_z + static_cast<double>(maximum);
        std::cout
            << std::setprecision(std::numeric_limits<float>::max_digits10)
            << "valid_local_height_min=" << minimum << '\n'
            << "valid_local_height_max=" << maximum << '\n'
            << std::setprecision(std::numeric_limits<double>::max_digits10)
            << "valid_absolute_z_min=" << absolute_minimum << '\n'
            << "valid_absolute_z_max=" << absolute_maximum << '\n';
    } else {
        std::cout
            << "valid_local_height_min=none\n"
            << "valid_local_height_max=none\n"
            << "valid_absolute_z_min=none\n"
            << "valid_absolute_z_max=none\n";
    }
}

/**
 * @brief Converts a compiler category into its stable process exit code.
 * @param code Compiler failure category.
 * @return Script-visible exit category.
 */
[[nodiscard]] ExitCode compiler_exit_code(
    const game_ex::world_compiler::CompilerErrorCode code) noexcept {
    switch (code) {
    case game_ex::world_compiler::CompilerErrorCode::io:
        return ExitCode::io;
    case game_ex::world_compiler::CompilerErrorCode::invalid_manifest:
        return ExitCode::invalid_manifest;
    case game_ex::world_compiler::CompilerErrorCode::invalid_staged_input:
        return ExitCode::invalid_staged_input;
    }
    return ExitCode::unexpected;
}

/**
 * @brief Converts a package-reader category into its stable process exit code.
 * @param code Reader failure category.
 * @return I/O for inaccessible files, otherwise invalid-package.
 */
[[nodiscard]] ExitCode package_exit_code(
    const game_ex::world_format::PackageErrorCode code) noexcept {
    return code == game_ex::world_format::PackageErrorCode::io
        ? ExitCode::io
        : ExitCode::invalid_package;
}

/**
 * @brief Executes one syntax-checked command.
 * @param argc Original argument count.
 * @param argv Original argument vector.
 * @return Stable process exit category.
 */
[[nodiscard]] ExitCode run_command(const int argc, const char* const argv[]) {
    const std::string_view command(argv[1]);
    if (command == "--help" && argc == 2) {
        print_help(std::cout);
        return ExitCode::success;
    }
    if (command == "--version" && argc == 2) {
        std::cout
            << "Game_EX World Compiler " << GAMEEX_VERSION_STRING << '\n'
            << "world_format=" << game_ex::world_format::current_major_version << '.'
            << game_ex::world_format::current_minor_version << '\n';
        return ExitCode::success;
    }
    if (command == "validate" && argc == 4
        && std::string_view(argv[2]) == "--manifest") {
        const auto package = game_ex::world_compiler::load_staging_world(argv[3]);
        std::cout << "status=valid\n";
        print_package_summary(package);
        return ExitCode::success;
    }
    if (command == "compile" && argc == 6
        && std::string_view(argv[2]) == "--manifest"
        && std::string_view(argv[4]) == "--output") {
        const std::filesystem::path output(argv[5]);
        game_ex::world_compiler::compile_world(argv[3], output);
        const auto package = game_ex::world_format::read_world_package(output);
        std::cout
            << "status=compiled\n"
            << "output=" << output.string() << '\n'
            << "checksum=0x" << std::hex << std::setw(8) << std::setfill('0')
            << package.checksum << std::dec << '\n';
        return ExitCode::success;
    }
    if (command == "inspect" && argc == 4
        && std::string_view(argv[2]) == "--package") {
        const auto package = game_ex::world_format::read_world_package(argv[3]);
        std::cout << "status=valid_package\n";
        print_package_summary(package);
        std::cout
            << "checksum=0x" << std::hex << std::setw(8) << std::setfill('0')
            << package.checksum << std::dec << '\n';
        return ExitCode::success;
    }

    std::cerr << "usage: invalid command or arguments; use --help\n";
    return ExitCode::usage;
}

} // namespace

/**
 * @brief Runs the stable world-compiler command-line interface.
 * @param argc Number of command-line arguments including the executable.
 * @param argv Null-terminated command-line argument strings.
 * @return A stable categorized process exit code documented by the CLI.
 */
int main(const int argc, const char* const argv[]) {
    if (argc < 2) {
        std::cerr << "usage: a command is required; use --help\n";
        return static_cast<int>(ExitCode::usage);
    }

    try {
        return static_cast<int>(run_command(argc, argv));
    } catch (const game_ex::world_compiler::CompilerError& error) {
        const ExitCode exit_code = compiler_exit_code(error.code());
        std::cerr << "compiler_error: " << error.what() << '\n';
        return static_cast<int>(exit_code);
    } catch (const game_ex::world_format::PackageError& error) {
        const ExitCode exit_code = package_exit_code(error.code());
        std::cerr << "package_error: " << error.what() << '\n';
        return static_cast<int>(exit_code);
    } catch (const std::exception& error) {
        std::cerr << "unexpected_error: " << error.what() << '\n';
        return static_cast<int>(ExitCode::unexpected);
    } catch (...) {
        std::cerr << "unexpected_error: non-standard exception\n";
        return static_cast<int>(ExitCode::unexpected);
    }
}
