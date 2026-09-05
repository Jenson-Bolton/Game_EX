/**
 * @file world_compiler_tests.cpp
 * @brief Dependency-free format, compiler, validation, and integrity tests.
 */

#include "game_ex/world_compiler/world_compiler.hpp"
#include "game_ex/world_format/world_package.hpp"
#include "compiler/package_encoding.hpp"
#include "world_format/package_layout.hpp"
#include "hex_fixture.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

/** Test failure carrying a concise assertion diagnostic. */
class TestFailure final : public std::runtime_error {
public:
    /**
     * @brief Creates a test failure.
     * @param message Failed expectation description.
     */
    explicit TestFailure(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief Requires one condition.
 * @param condition Condition that must hold.
 * @param message Failure diagnostic.
 */
void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw TestFailure(message);
    }
}

/**
 * @brief Owns and cleans one unique test directory.
 */
class TemporaryDirectory final {
public:
    /** Creates a unique directory beneath the operating-system temporary root. */
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("gameex_world_compiler_tests_" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    /** Removes all test-owned files without propagating cleanup failure. */
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    /**
     * @brief Returns the owned directory path.
     * @return Stable directory path.
     */
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    /** Unique test-owned directory. */
    std::filesystem::path path_;
};

/**
 * @brief Reads a complete binary file.
 * @param path File to read.
 * @return Exact bytes.
 */
[[nodiscard]] std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    if (!bytes.empty()) {
        stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw TestFailure("Could not read test file: " + path.string());
    }
    return bytes;
}

/**
 * @brief Independently decodes one little-endian 32-bit test value.
 * @param bytes Complete test byte sequence.
 * @param offset First byte to decode.
 * @return Decoded unsigned value.
 */
[[nodiscard]] std::uint32_t read_u32_le(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset) {
    expect(offset <= bytes.size() && bytes.size() - offset >= sizeof(std::uint32_t),
        "Test u32 offset is outside encoded bytes");
    std::uint32_t value = 0;
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        value |= static_cast<std::uint32_t>(bytes[offset + shift / 8U]) << shift;
    }
    return value;
}

/**
 * @brief Independently decodes one little-endian 64-bit test value.
 * @param bytes Complete test byte sequence.
 * @param offset First byte to decode.
 * @return Decoded unsigned value.
 */
[[nodiscard]] std::uint64_t read_u64_le(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset) {
    expect(offset <= bytes.size() && bytes.size() - offset >= sizeof(std::uint64_t),
        "Test u64 offset is outside encoded bytes");
    std::uint64_t value = 0;
    for (unsigned int shift = 0; shift < 64U; shift += 8U) {
        value |= static_cast<std::uint64_t>(bytes[offset + shift / 8U]) << shift;
    }
    return value;
}

/**
 * @brief Locates one zero-based metadata string's first data byte.
 * @param bytes Complete canonical package bytes.
 * @param ordinal Zero-based string position in the fixed metadata order.
 * @return Offset immediately after that string's u32 length.
 */
[[nodiscard]] std::size_t metadata_string_data_offset(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t ordinal) {
    std::size_t cursor = static_cast<std::size_t>(read_u64_le(bytes, 96))
        + 3U * sizeof(std::uint64_t);
    for (std::size_t index = 0; index <= ordinal; ++index) {
        const std::uint32_t length = read_u32_le(bytes, cursor);
        cursor += sizeof(std::uint32_t);
        if (index == ordinal) {
            expect(static_cast<std::size_t>(length) <= bytes.size() - cursor,
                "Metadata string extends beyond package bytes");
            return cursor;
        }
        expect(static_cast<std::size_t>(length) <= bytes.size() - cursor,
            "Metadata scan extends beyond package bytes");
        cursor += length;
    }
    throw TestFailure("Metadata ordinal was not reachable");
}

/**
 * @brief Replaces one metadata string without changing its encoded length.
 * @param bytes Complete mutable package bytes.
 * @param ordinal Zero-based fixed metadata-string ordinal.
 * @param replacement Equal-length replacement bytes.
 */
void replace_metadata_string(
    std::vector<std::uint8_t>& bytes,
    const std::size_t ordinal,
    const std::string_view replacement) {
    const std::size_t offset = metadata_string_data_offset(bytes, ordinal);
    const std::uint32_t encoded_length = read_u32_le(bytes, offset - sizeof(std::uint32_t));
    expect(replacement.size() == encoded_length,
        "Semantic mutation must preserve metadata length");
    for (std::size_t index = 0; index < replacement.size(); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            static_cast<unsigned char>(replacement[index]));
    }
}

/**
 * @brief Writes exact binary bytes, replacing a test-owned file.
 * @param path File to replace.
 * @param bytes Exact bytes.
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
        throw TestFailure("Could not write test file: " + path.string());
    }
}

/**
 * @brief Reads a complete test text file without newline conversion.
 * @param path File to read.
 * @return Exact bytes in string storage.
 */
[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_bytes(path);
    std::string text;
    text.reserve(bytes.size());
    for (const std::uint8_t byte : bytes) {
        text.push_back(static_cast<char>(byte));
    }
    return text;
}

/**
 * @brief Writes exact test text bytes.
 * @param path File to replace.
 * @param text Exact bytes.
 */
void write_text(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        throw TestFailure("Could not write test text: " + path.string());
    }
}

/**
 * @brief Copies the canonical synthetic fixture into a mutable test stage.
 * @param destination New stage directory.
 */
void copy_fixture(const std::filesystem::path& destination) {
    const std::filesystem::path source(GAMEEX_WORLD_FIXTURE_DIR);
    std::filesystem::create_directories(destination);
    for (const std::string_view filename : {
             "manifest.gexstage", "heights.txt", "validity.txt", "synthetic_source.txt"}) {
        std::filesystem::copy_file(
            source / filename,
            destination / filename,
            std::filesystem::copy_options::none);
    }
}

/**
 * @brief Replaces one required manifest value in test-owned text.
 * @param manifest Manifest path.
 * @param key Exact manifest key.
 * @param replacement Replacement value without newline.
 */
void replace_manifest_value(
    const std::filesystem::path& manifest,
    const std::string_view key,
    const std::string_view replacement) {
    std::string text = read_text(manifest);
    const std::string marker = std::string(key) + '=';
    std::size_t begin = text.find(marker);
    while (begin != std::string::npos && begin != 0 && text[begin - 1] != '\n') {
        begin = text.find(marker, begin + 1U);
    }
    if (begin == std::string::npos) {
        throw TestFailure("Manifest replacement key was not found: " + std::string(key));
    }
    const std::size_t value_begin = begin + marker.size();
    const std::size_t end = text.find('\n', value_begin);
    text.replace(value_begin, end - value_begin, replacement);
    write_text(manifest, text);
}

/**
 * @brief Removes one required manifest line from test-owned text.
 * @param manifest Manifest path.
 * @param key Exact manifest key.
 */
void remove_manifest_field(
    const std::filesystem::path& manifest,
    const std::string_view key) {
    std::string text = read_text(manifest);
    const std::string marker = std::string(key) + '=';
    const std::size_t begin = text.find(marker);
    if (begin == std::string::npos || (begin != 0 && text[begin - 1] != '\n')) {
        throw TestFailure("Manifest removal key was not found: " + std::string(key));
    }
    const std::size_t line_end = text.find('\n', begin);
    text.erase(begin, line_end == std::string::npos ? text.size() - begin : line_end + 1U - begin);
    write_text(manifest, text);
}

/**
 * @brief Requires a classified compiler failure.
 * @tparam Callable Invocable operation type.
 * @param expected_code Required category.
 * @param operation Operation expected to throw.
 */
template <typename Callable>
void expect_compiler_error(
    const game_ex::world_compiler::CompilerErrorCode expected_code,
    Callable&& operation) {
    try {
        operation();
    } catch (const game_ex::world_compiler::CompilerError& error) {
        expect(error.code() == expected_code, "Compiler error category did not match");
        return;
    }
    throw TestFailure("Expected CompilerError was not thrown");
}

/**
 * @brief Requires a classified package-reader failure.
 * @tparam Callable Invocable operation type.
 * @param expected_code Required category.
 * @param operation Operation expected to throw.
 */
template <typename Callable>
void expect_package_error(
    const game_ex::world_format::PackageErrorCode expected_code,
    Callable&& operation) {
    try {
        operation();
    } catch (const game_ex::world_format::PackageError& error) {
        expect(error.code() == expected_code, "Package error category did not match");
        return;
    }
    throw TestFailure("Expected PackageError was not thrown");
}

/**
 * @brief Requires every provenance value to survive package encoding.
 * @param expected Compiler-side logical values.
 * @param actual Reader-side decoded values.
 */
void expect_equal_provenance(
    const game_ex::world_format::Provenance& expected,
    const game_ex::world_format::Provenance& actual) {
    expect(expected.provider == actual.provider, "Provider did not round trip");
    expect(expected.product == actual.product, "Product did not round trip");
    expect(expected.edition == actual.edition, "Edition did not round trip");
    expect(expected.source_uri == actual.source_uri, "Source URI did not round trip");
    expect(expected.acquisition_timestamp == actual.acquisition_timestamp,
        "Acquisition timestamp did not round trip");
    expect(expected.licence_name == actual.licence_name, "Licence name did not round trip");
    expect(expected.licence_uri == actual.licence_uri, "Licence URI did not round trip");
    expect(expected.attribution == actual.attribution, "Attribution did not round trip");
    expect(expected.rights_statement == actual.rights_statement, "Rights did not round trip");
    expect(expected.source_filename == actual.source_filename, "Source filename did not round trip");
    expect(expected.source_byte_size == actual.source_byte_size, "Source size did not round trip");
    expect(expected.source_sha256 == actual.source_sha256, "Source SHA-256 did not round trip");
    expect(expected.source_classification == actual.source_classification,
        "Source classification did not round trip");
    expect(expected.stage_classification == actual.stage_classification,
        "Stage classification did not round trip");
    expect(expected.source_crs == actual.source_crs, "Source CRS did not round trip");
    expect(expected.source_axis_order == actual.source_axis_order,
        "Source axis order did not round trip");
    expect(expected.source_horizontal_units == actual.source_horizontal_units,
        "Source horizontal units did not round trip");
    expect(expected.source_vertical_reference == actual.source_vertical_reference,
        "Source vertical reference did not round trip");
    expect(expected.source_vertical_units == actual.source_vertical_units,
        "Source vertical units did not round trip");
    expect(expected.source_no_data_convention == actual.source_no_data_convention,
        "Source no-data did not round trip");
    expect(expected.stage_axis_order == actual.stage_axis_order,
        "Stage axis order did not round trip");
    expect(expected.stage_horizontal_units == actual.stage_horizontal_units,
        "Stage horizontal units did not round trip");
    expect(expected.stage_vertical_reference == actual.stage_vertical_reference,
        "Stage vertical reference did not round trip");
    expect(expected.stage_vertical_units == actual.stage_vertical_units,
        "Stage vertical units did not round trip");
    expect(expected.stage_no_data_convention == actual.stage_no_data_convention,
        "Stage no-data did not round trip");
    expect(expected.resolution == actual.resolution, "Resolution did not round trip");
    expect(expected.bounds == actual.bounds, "Bounds statement did not round trip");
    expect(expected.epoch == actual.epoch, "Epoch did not round trip");
    expect(expected.transformation == actual.transformation, "Transformation did not round trip");
    expect(expected.responsible_owner == actual.responsible_owner, "Owner did not round trip");
    expect(expected.staged_heights_path == actual.staged_heights_path,
        "Staged heights path did not round trip");
    expect(expected.staged_heights_byte_size == actual.staged_heights_byte_size,
        "Staged heights size did not round trip");
    expect(expected.staged_heights_sha256 == actual.staged_heights_sha256,
        "Staged heights SHA-256 did not round trip");
    expect(expected.staged_validity_path == actual.staged_validity_path,
        "Staged validity path did not round trip");
    expect(expected.staged_validity_byte_size == actual.staged_validity_byte_size,
        "Staged validity size did not round trip");
    expect(expected.staged_validity_sha256 == actual.staged_validity_sha256,
        "Staged validity SHA-256 did not round trip");
}

/** Exercises manifest loading, byte determinism, and complete reader round trip. */
void test_round_trip_and_determinism() {
    TemporaryDirectory temporary;
    const auto stage_a = temporary.path() / "stage_a";
    const auto stage_b = temporary.path() / "stage_b";
    copy_fixture(stage_a);
    copy_fixture(stage_b);

    const auto logical = game_ex::world_compiler::load_staging_world(
        stage_a / "manifest.gexstage");
    expect(logical.header.minor_version == 2, "Concrete serialized format must be version 0.2");
    expect(logical.terrain.origin_x == -742000.125, "Negative EPSG:5514 X was not retained");
    expect(logical.terrain.origin_y == -1045000.5, "Negative EPSG:5514 Y was not retained");
    expect(logical.terrain.origin_z == 250.25, "Vertical datum origin was not retained");
    expect(logical.terrain.heights == std::vector<float>({0.0F, 1.5F, -2.0F, 4.0F}),
        "Tile-local height order is incorrect");
    expect(logical.terrain.validity == std::vector<std::uint8_t>({1U, 1U, 0U, 1U}),
        "Validity-mask order is incorrect");
    expect(logical.terrain.origin_x + logical.terrain.spacing_x
            * static_cast<double>(logical.terrain.columns - 1U) == -741998.125,
        "Derived +X tile bound is incorrect");
    expect(logical.terrain.origin_z + static_cast<double>(logical.terrain.heights[3]) == 254.25,
        "Absolute Z formula is incorrect");

    const auto output_a = stage_a / "world.gexworld";
    const auto output_b = stage_b / "world.gexworld";
    game_ex::world_compiler::compile_world(stage_a / "manifest.gexstage", output_a);
    game_ex::world_compiler::compile_world(stage_b / "manifest.gexstage", output_b);
    const std::vector<std::uint8_t> encoded = read_bytes(output_a);
    expect(encoded == read_bytes(output_b),
        "Identical stages in different directories must produce identical bytes");
    const std::vector<std::uint8_t> golden = game_ex::world_test_detail::read_hex_fixture(
        std::filesystem::path(GAMEEX_WORLD_FIXTURE_DIR) / "expected.gexworld.hex");
    expect(encoded == golden,
        "Compiler output differs from the complete checked-in WorldFormat 0.2 golden");
    expect(encoded.size() == 1'151U, "Synthetic package size changed unexpectedly");
    expect(encoded[8] == 0U && encoded[9] == 0U
            && encoded[10] == 2U && encoded[11] == 0U,
        "World-format version is not explicit little-endian 0.2");
    expect(encoded[12] == 128U && encoded[13] == 0U
            && encoded[14] == 0U && encoded[15] == 0U,
        "Header size is not explicit little-endian 128");
    expect(read_u64_le(encoded, 16) == encoded.size(),
        "Encoded file size is not canonical little-endian data");
    expect(read_u32_le(encoded, 24) != 0U, "Encoded CRC field is empty");
    expect(read_u32_le(encoded, 28) == 0U, "Encoded flags are not zero");
    expect(read_u32_le(encoded, 32) == 5'514U, "Encoded EPSG code is incorrect");
    expect(read_u32_le(encoded, 36) == 2U && read_u32_le(encoded, 40) == 2U,
        "Encoded dimensions are incorrect");
    expect(read_u32_le(encoded, 44) == 35U, "Encoded metadata count is incorrect");
    expect(read_u64_le(encoded, 80) == 128U, "Height offset is not canonical");
    expect(read_u64_le(encoded, 88) == 144U, "Validity offset is not canonical");
    expect(read_u64_le(encoded, 96) == 148U, "Metadata offset is not canonical");
    expect(read_u64_le(encoded, 104) == 1'003U, "Metadata size is not canonical");
    expect(read_u64_le(encoded, 112) == 4U, "Encoded sample count is incorrect");
    constexpr std::array<std::uint8_t, 16> expected_height_bytes{
        0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0xC0U, 0x3FU,
        0x00U, 0x00U, 0x00U, 0xC0U,
        0x00U, 0x00U, 0x80U, 0x40U};
    expect(std::equal(expected_height_bytes.begin(), expected_height_bytes.end(),
               encoded.begin() + 128),
        "Height payload is not canonical little-endian binary32 data");
    expect(encoded[144] == 1U && encoded[145] == 1U
            && encoded[146] == 0U && encoded[147] == 1U,
        "Validity payload is not in canonical row-major order");
    expect(read_u64_le(encoded, 148) == 34U
            && read_u64_le(encoded, 156) == 11U
            && read_u64_le(encoded, 164) == 8U,
        "Metadata sizes are not encoded in their canonical order");

    const auto decoded = game_ex::world_format::read_world_package(output_a);
    expect(decoded.header.magic == game_ex::world_format::world_file_magic,
        "Decoded package magic is incorrect");
    expect(decoded.terrain.layer_id == logical.terrain.layer_id, "Layer ID did not round trip");
    expect(decoded.terrain.tile_id == logical.terrain.tile_id, "Tile ID did not round trip");
    expect(decoded.terrain.columns == logical.terrain.columns, "Columns did not round trip");
    expect(decoded.terrain.rows == logical.terrain.rows, "Rows did not round trip");
    expect(decoded.terrain.origin_x == logical.terrain.origin_x, "Origin X did not round trip");
    expect(decoded.terrain.origin_y == logical.terrain.origin_y, "Origin Y did not round trip");
    expect(decoded.terrain.origin_z == logical.terrain.origin_z, "Origin Z did not round trip");
    expect(decoded.terrain.spacing_x == logical.terrain.spacing_x, "Spacing X did not round trip");
    expect(decoded.terrain.spacing_y == logical.terrain.spacing_y, "Spacing Y did not round trip");
    expect(decoded.terrain.heights == logical.terrain.heights, "Heights did not round trip");
    expect(decoded.terrain.validity == logical.terrain.validity, "Validity did not round trip");
    expect(decoded.checksum != 0U, "Encoded package checksum must be populated");
    expect_equal_provenance(logical.provenance, decoded.provenance);

    expect_compiler_error(game_ex::world_compiler::CompilerErrorCode::io, [&] {
        game_ex::world_compiler::compile_world(stage_a / "manifest.gexstage", output_a);
    });
    expect(!std::filesystem::exists(output_a.string() + ".part"),
        "Refused overwrite must not leave a partial file");
}

/** Exercises the strict exact-key manifest grammar and numeric bounds. */
void test_manifest_validation() {
    TemporaryDirectory temporary;
    const auto run_case = [&](const std::string_view name, const auto& mutate) {
        const auto stage = temporary.path() / name;
        copy_fixture(stage);
        mutate(stage / "manifest.gexstage");
        expect_compiler_error(game_ex::world_compiler::CompilerErrorCode::invalid_manifest, [&] {
            static_cast<void>(
                game_ex::world_compiler::load_staging_world(stage / "manifest.gexstage"));
        });
    };

    const auto crlf_stage = temporary.path() / "crlf";
    copy_fixture(crlf_stage);
    const auto crlf_manifest = crlf_stage / "manifest.gexstage";
    const std::string lf_text = read_text(crlf_manifest);
    std::string crlf_text;
    crlf_text.reserve(lf_text.size() * 2U);
    for (const char byte : lf_text) {
        if (byte == '\n') {
            crlf_text += "\r\n";
        } else {
            crlf_text.push_back(byte);
        }
    }
    write_text(crlf_manifest, crlf_text);
    static_cast<void>(game_ex::world_compiler::load_staging_world(crlf_manifest));

    run_case("missing", [](const auto& manifest) {
        remove_manifest_field(manifest, "provenance.provider");
    });
    run_case("unknown", [](const auto& manifest) {
        std::string text = read_text(manifest);
        text += "unknown.key=value\n";
        write_text(manifest, text);
    });
    run_case("duplicate", [](const auto& manifest) {
        std::string text = read_text(manifest);
        text += "terrain.tile.rows=2\n";
        write_text(manifest, text);
    });
    run_case("wrong_crs", [](const auto& manifest) {
        replace_manifest_value(manifest, "crs", "EPSG:4326");
    });
    run_case("dimension", [](const auto& manifest) {
        replace_manifest_value(manifest, "terrain.tile.columns", "4097");
    });
    run_case("sample_count", [](const auto& manifest) {
        replace_manifest_value(manifest, "terrain.tile.columns", "4096");
        replace_manifest_value(manifest, "terrain.tile.rows", "4096");
    });
    run_case("overflow", [](const auto& manifest) {
        replace_manifest_value(manifest, "terrain.tile.columns", "4096");
        replace_manifest_value(manifest, "terrain.tile.spacing_x", "1e308");
    });
    run_case("non_finite", [](const auto& manifest) {
        replace_manifest_value(manifest, "terrain.tile.origin_x", "nan");
    });
    run_case("bad_date", [](const auto& manifest) {
        replace_manifest_value(manifest, "provenance.acquired_utc", "2025-02-29T00:00:00Z");
    });
    run_case("bad_sha", [](const auto& manifest) {
        replace_manifest_value(manifest, "provenance.source_sha256", "ABC");
    });
    run_case("long_identifier", [](const auto& manifest) {
        replace_manifest_value(manifest, "terrain.tile_id", std::string(65, 'a'));
    });
    run_case("invalid_utf8", [](const auto& manifest) {
        std::string invalid_value = "Game_EX ";
        invalid_value.push_back(static_cast<char>(0x80U));
        replace_manifest_value(manifest, "provenance.provider", invalid_value);
    });
    run_case("lone_cr", [](const auto& manifest) {
        std::string text = read_text(manifest);
        text += "# rejected lone carriage return";
        text.push_back('\r');
        write_text(manifest, text);
    });
    run_case("del_comment", [](const auto& manifest) {
        std::string text = read_text(manifest);
        text += "# rejected DEL ";
        text.push_back(static_cast<char>(0x7FU));
        text.push_back('\n');
        write_text(manifest, text);
    });
}

/** Exercises path containment and staged byte-size/hash binding. */
void test_staged_input_validation() {
    TemporaryDirectory temporary;

    const auto unicode = temporary.path() / "unicode";
    copy_fixture(unicode);
    constexpr std::u8string_view unicode_name_u8{u8"výšky.txt"};
    std::string unicode_name;
    unicode_name.reserve(unicode_name_u8.size());
    for (const char8_t byte : unicode_name_u8) {
        unicode_name.push_back(std::bit_cast<char>(byte));
    }
    std::filesystem::rename(
        unicode / "heights.txt",
        unicode / std::filesystem::path{std::u8string{unicode_name_u8}});
    const auto unicode_manifest = unicode / "manifest.gexstage";
    replace_manifest_value(unicode_manifest, "terrain.tile.heights", unicode_name);
    const auto unicode_output = unicode / "world.gexworld";
    game_ex::world_compiler::compile_world(unicode_manifest, unicode_output);
    const auto unicode_package = game_ex::world_format::read_world_package(unicode_output);
    expect(unicode_package.provenance.staged_heights_path == unicode_name,
        "UTF-8 staged filename was not preserved in package provenance");
    expect(unicode_package.terrain.heights
            == std::vector<float>({0.0F, 1.5F, -2.0F, 4.0F}),
        "UTF-8 staged filename did not resolve to the expected height data");

    const auto traversal = temporary.path() / "traversal";
    copy_fixture(traversal);
    replace_manifest_value(
        traversal / "manifest.gexstage", "terrain.tile.heights", "../heights.txt");
    expect_compiler_error(
        game_ex::world_compiler::CompilerErrorCode::invalid_staged_input, [&] {
            static_cast<void>(
                game_ex::world_compiler::load_staging_world(
                    traversal / "manifest.gexstage"));
        });

    const auto absolute = temporary.path() / "absolute";
    copy_fixture(absolute);
    replace_manifest_value(
        absolute / "manifest.gexstage", "terrain.tile.heights", "C:/escape.txt");
    expect_compiler_error(
        game_ex::world_compiler::CompilerErrorCode::invalid_staged_input, [&] {
            static_cast<void>(game_ex::world_compiler::load_staging_world(
                absolute / "manifest.gexstage"));
        });

    const auto tampered = temporary.path() / "tampered";
    copy_fixture(tampered);
    write_text(tampered / "heights.txt", "9 1.5\n-2 4\n");
    const auto failed_output = tampered / "failed.gexworld";
    expect_compiler_error(
        game_ex::world_compiler::CompilerErrorCode::invalid_staged_input, [&] {
            game_ex::world_compiler::compile_world(
                tampered / "manifest.gexstage", failed_output);
        });
    expect(!std::filesystem::exists(failed_output),
        "Invalid staged input must not create an output package");

    const auto tampered_validity = temporary.path() / "tampered_validity";
    copy_fixture(tampered_validity);
    write_text(tampered_validity / "validity.txt", "1 0\n0 1\n");
    expect_compiler_error(
        game_ex::world_compiler::CompilerErrorCode::invalid_staged_input, [&] {
            static_cast<void>(game_ex::world_compiler::load_staging_world(
                tampered_validity / "manifest.gexstage"));
        });

    const auto size_mismatch = temporary.path() / "size_mismatch";
    copy_fixture(size_mismatch);
    replace_manifest_value(
        size_mismatch / "manifest.gexstage", "staged.validity.byte_size", "7");
    expect_compiler_error(
        game_ex::world_compiler::CompilerErrorCode::invalid_staged_input, [&] {
            static_cast<void>(
                game_ex::world_compiler::load_staging_world(
                    size_mismatch / "manifest.gexstage"));
        });
}

/** Exercises lock/part refusal, cleanup, and a controlled same-output race. */
void test_atomic_publication() {
    TemporaryDirectory temporary;
    const auto stage = temporary.path() / "stage";
    copy_fixture(stage);
    const auto manifest = stage / "manifest.gexstage";

    const auto locked_output = stage / "locked.gexworld";
    const std::filesystem::path locked_lock(locked_output.string() + ".lock");
    std::filesystem::create_directory(locked_lock);
    expect_compiler_error(game_ex::world_compiler::CompilerErrorCode::io, [&] {
        game_ex::world_compiler::compile_world(manifest, locked_output);
    });
    expect(std::filesystem::is_directory(locked_lock),
        "A reservation not owned by this compiler must be preserved");
    expect(!std::filesystem::exists(locked_output)
            && !std::filesystem::exists(locked_output.string() + ".part"),
        "Lock refusal must not create package artifacts");

    const auto partial_output = stage / "partial.gexworld";
    const std::filesystem::path partial_path(partial_output.string() + ".part");
    write_text(partial_path, "pre-existing partial sentinel");
    expect_compiler_error(game_ex::world_compiler::CompilerErrorCode::io, [&] {
        game_ex::world_compiler::compile_world(manifest, partial_output);
    });
    expect(read_text(partial_path) == "pre-existing partial sentinel",
        "A partial file not owned by this compiler must be preserved");
    expect(!std::filesystem::exists(partial_output.string() + ".lock"),
        "Part refusal must release the compiler-owned reservation");

    const auto clean_output = stage / "clean.gexworld";
    game_ex::world_compiler::compile_world(manifest, clean_output);
    expect(std::filesystem::exists(clean_output)
            && !std::filesystem::exists(clean_output.string() + ".part")
            && !std::filesystem::exists(clean_output.string() + ".lock"),
        "Successful publication must remove its part and reservation artifacts");

    const auto raced_output = stage / "raced.gexworld";
    std::atomic<unsigned int> successes{0U};
    std::atomic<unsigned int> refusals{0U};
    std::atomic<unsigned int> unexpected{0U};
    std::latch ready(2);
    std::latch start(1);
    const auto compile_racer = [&] {
        ready.count_down();
        start.wait();
        try {
            game_ex::world_compiler::compile_world(manifest, raced_output);
            successes.fetch_add(1U, std::memory_order_relaxed);
        } catch (const game_ex::world_compiler::CompilerError& error) {
            if (error.code() == game_ex::world_compiler::CompilerErrorCode::io) {
                refusals.fetch_add(1U, std::memory_order_relaxed);
            } else {
                unexpected.fetch_add(1U, std::memory_order_relaxed);
            }
        } catch (...) {
            unexpected.fetch_add(1U, std::memory_order_relaxed);
        }
    };
    std::thread first(compile_racer);
    std::thread second(compile_racer);
    ready.wait();
    start.count_down();
    first.join();
    second.join();
    expect(successes.load(std::memory_order_relaxed) == 1U
            && refusals.load(std::memory_order_relaxed) == 1U
            && unexpected.load(std::memory_order_relaxed) == 0U,
        "Exactly one same-output compiler must publish and one must fail with I/O");
    static_cast<void>(game_ex::world_format::read_world_package(raced_output));
    expect(!std::filesystem::exists(raced_output.string() + ".part")
            && !std::filesystem::exists(raced_output.string() + ".lock"),
        "Concurrent publication must leave no owned artifacts");
}

/**
 * @brief Writes a little-endian CRC value into the fixed checksum field.
 * @param bytes Mutable complete package bytes whose checksum field is zero.
 */
void patch_package_checksum(std::vector<std::uint8_t>& bytes) {
    const std::uint32_t crc = game_ex::world_format::detail::crc32_iso_hdlc(bytes);
    for (std::size_t index = 0; index < sizeof(crc); ++index) {
        const unsigned int shift = static_cast<unsigned int>(index * 8U);
        bytes[game_ex::world_format::detail::checksum_offset + index]
            = static_cast<std::uint8_t>((crc >> shift) & 0xFFU);
    }
}

/** Exercises fixed stage semantics in both the encoder and CRC-valid reader path. */
void test_fixed_stage_semantics() {
    TemporaryDirectory temporary;
    const auto stage = temporary.path() / "stage";
    copy_fixture(stage);
    const auto manifest = stage / "manifest.gexstage";
    const auto logical = game_ex::world_compiler::load_staging_world(manifest);

    struct EncoderMutation final {
        /** Provenance field to change. */
        std::string game_ex::world_format::Provenance::* field;
        /** Equal-purpose invalid semantic value. */
        std::string_view replacement;
    };
    const std::array<EncoderMutation, 4> encoder_mutations{
        EncoderMutation{&game_ex::world_format::Provenance::stage_axis_order, "Y,X"},
        EncoderMutation{&game_ex::world_format::Provenance::stage_horizontal_units, "meter"},
        EncoderMutation{&game_ex::world_format::Provenance::stage_vertical_units, "meter"},
        EncoderMutation{&game_ex::world_format::Provenance::stage_no_data_convention,
            "validity_musk"}};
    for (const EncoderMutation& mutation : encoder_mutations) {
        auto changed = logical;
        changed.provenance.*(mutation.field) = mutation.replacement;
        expect_package_error(game_ex::world_format::PackageErrorCode::invalid_format, [&] {
            static_cast<void>(game_ex::world_format::detail::encode_world_package(changed));
        });
    }

    const auto canonical_path = stage / "canonical.gexworld";
    game_ex::world_compiler::compile_world(manifest, canonical_path);
    const std::vector<std::uint8_t> canonical = read_bytes(canonical_path);
    struct ReaderMutation final {
        /** Zero-based string position in the fixed metadata order. */
        std::size_t ordinal;
        /** Equal-length invalid semantic value. */
        std::string_view replacement;
    };
    const std::array<ReaderMutation, 4> reader_mutations{
        ReaderMutation{21U, "Y,X"},
        ReaderMutation{22U, "meter"},
        ReaderMutation{24U, "meter"},
        ReaderMutation{25U, "validity_musk"}};
    for (std::size_t index = 0; index < reader_mutations.size(); ++index) {
        auto changed = canonical;
        replace_metadata_string(
            changed, reader_mutations[index].ordinal, reader_mutations[index].replacement);
        std::fill_n(
            changed.begin() + game_ex::world_format::detail::checksum_offset,
            sizeof(std::uint32_t),
            std::uint8_t{0});
        patch_package_checksum(changed);
        const auto changed_path = stage
            / ("invalid_semantics_" + std::to_string(index) + ".gexworld");
        write_bytes(changed_path, changed);
        expect_package_error(game_ex::world_format::PackageErrorCode::invalid_format, [&] {
            static_cast<void>(game_ex::world_format::read_world_package(changed_path));
        });
    }
}

/** Exercises known CRC behavior plus malformed, truncated, and corrupt packages. */
void test_package_validation_and_integrity() {
    constexpr std::array<std::uint8_t, 9> crc_vector{
        '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    expect(game_ex::world_format::detail::crc32_iso_hdlc(crc_vector) == 0xCBF43926U,
        "CRC-32/ISO-HDLC known vector did not match");

    TemporaryDirectory temporary;
    const auto stage = temporary.path() / "stage";
    copy_fixture(stage);
    const auto package_path = stage / "world.gexworld";
    game_ex::world_compiler::compile_world(stage / "manifest.gexstage", package_path);
    const std::vector<std::uint8_t> original = read_bytes(package_path);

    auto corrupt = original;
    corrupt[128] ^= 0x01U;
    const auto corrupt_path = stage / "corrupt.gexworld";
    write_bytes(corrupt_path, corrupt);
    expect_package_error(game_ex::world_format::PackageErrorCode::integrity, [&] {
        static_cast<void>(game_ex::world_format::read_world_package(corrupt_path));
    });

    auto truncated = original;
    truncated.resize(64);
    const auto truncated_path = stage / "truncated.gexworld";
    write_bytes(truncated_path, truncated);
    expect_package_error(game_ex::world_format::PackageErrorCode::invalid_format, [&] {
        static_cast<void>(game_ex::world_format::read_world_package(truncated_path));
    });

    truncated = original;
    truncated.pop_back();
    const auto payload_truncated_path = stage / "payload_truncated.gexworld";
    write_bytes(payload_truncated_path, truncated);
    expect_package_error(game_ex::world_format::PackageErrorCode::integrity, [&] {
        static_cast<void>(game_ex::world_format::read_world_package(payload_truncated_path));
    });

    auto unsupported = original;
    unsupported[10] = 3U;
    unsupported[11] = 0U;
    for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
        unsupported[game_ex::world_format::detail::checksum_offset + index] = 0U;
    }
    patch_package_checksum(unsupported);
    const auto unsupported_path = stage / "unsupported.gexworld";
    write_bytes(unsupported_path, unsupported);
    expect_package_error(game_ex::world_format::PackageErrorCode::unsupported_version, [&] {
        static_cast<void>(game_ex::world_format::read_world_package(unsupported_path));
    });

    auto bad_offset = original;
    bad_offset[80] = 127U;
    for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
        bad_offset[game_ex::world_format::detail::checksum_offset + index] = 0U;
    }
    patch_package_checksum(bad_offset);
    const auto offset_path = stage / "bad_offset.gexworld";
    write_bytes(offset_path, bad_offset);
    expect_package_error(game_ex::world_format::PackageErrorCode::invalid_format, [&] {
        static_cast<void>(game_ex::world_format::read_world_package(offset_path));
    });

    expect_package_error(game_ex::world_format::PackageErrorCode::io, [&] {
        static_cast<void>(game_ex::world_format::read_world_package(stage / "missing.gexworld"));
    });
}

} // namespace

/**
 * @brief Runs all dependency-free world-compiler tests.
 * @return Zero on success, one with a diagnostic on failure.
 */
int main() {
    try {
        test_round_trip_and_determinism();
        test_manifest_validation();
        test_staged_input_validation();
        test_atomic_publication();
        test_fixed_stage_semantics();
        test_package_validation_and_integrity();
        std::cout << "All world compiler tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "World compiler test failure: " << error.what() << '\n';
        return 1;
    }
}
