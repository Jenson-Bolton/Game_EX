/**
 * @file package_encoder.cpp
 * @brief Offline-only explicit little-endian `.gexworld` encoder.
 */

#include "package_encoding.hpp"

#include "world_format/package_layout.hpp"
#include "world_format/package_validation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace game_ex::world_format::detail {
namespace {

/**
 * @brief Throws the standard encoder format error.
 * @param message Human-readable failed invariant.
 */
[[noreturn]] void invalid(const std::string& message) {
    throw PackageError(PackageErrorCode::invalid_format, message);
}

/**
 * @brief Validates one complete UTF-8 byte sequence.
 * @param value Bytes to inspect.
 * @return True only for shortest-form Unicode scalar encodings.
 */
[[nodiscard]] bool is_valid_utf8(const std::string_view value) noexcept {
    std::size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<std::uint8_t>(
            static_cast<unsigned char>(value[index]));
        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        std::size_t length = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xC2U && first <= 0xDFU) {
            length = 2;
            code_point = first & 0x1FU;
            minimum = 0x80U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            length = 3;
            code_point = first & 0x0FU;
            minimum = 0x800U;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            length = 4;
            code_point = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (length > value.size() - index) {
            return false;
        }
        for (std::size_t continuation = 1; continuation < length; ++continuation) {
            const auto byte = static_cast<std::uint8_t>(
                static_cast<unsigned char>(value[index + continuation]));
            if ((byte & 0xC0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (byte & 0x3FU);
        }
        if (code_point < minimum || code_point > 0x10FFFFU
            || (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
            return false;
        }
        index += length;
    }
    return true;
}

/**
 * @brief Validates bounded non-empty UTF-8 metadata.
 * @param value Metadata value.
 * @param field Human-readable field name.
 */
void validate_text(const std::string& value, const std::string_view field) {
    if (value.empty() || value.size() > maximum_metadata_string_bytes) {
        invalid(std::string(field) + " is empty or exceeds its byte bound");
    }
    if (!is_valid_utf8(value)) {
        invalid(std::string(field) + " is not valid UTF-8");
    }
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte == 0x7FU) {
            invalid(std::string(field) + " contains a control byte");
        }
    }
}

/**
 * @brief Validates one stable layer or tile identifier.
 * @param value Identifier value.
 * @param field Human-readable field name.
 */
void validate_identifier(const std::string& value, const std::string_view field) {
    validate_text(value, field);
    if (value.size() > 64U) {
        invalid(std::string(field) + " exceeds 64 bytes");
    }
    const bool valid = std::all_of(value.begin(), value.end(), [](const char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9') || byte == '_' || byte == '-'
            || byte == '.';
    });
    if (!valid) {
        invalid(std::string(field) + " contains a non-portable identifier byte");
    }
}

/**
 * @brief Validates one lowercase SHA-256 spelling.
 * @param value Digest value.
 * @param field Human-readable field name.
 */
void validate_sha256(const std::string& value, const std::string_view field) {
    if (value.size() != 64U || !std::all_of(
            value.begin(), value.end(), [](const char byte) {
                return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
            })) {
        invalid(std::string(field) + " must contain 64 lowercase hexadecimal digits");
    }
}

/**
 * @brief Validates the restricted path-free original source name.
 * @param value Name to inspect.
 */
void validate_source_filename(const std::string& value) {
    validate_text(value, "Source filename");
    if (value == "." || value == ".." || !std::all_of(
            value.begin(), value.end(), [](const char byte) {
                return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
                    || (byte >= '0' && byte <= '9') || byte == '.' || byte == '_'
                    || byte == '-';
            })) {
        invalid("Source filename is not a restricted path-free name");
    }
}

/**
 * @brief Validates one portable relative staged path lexically.
 * @param value Path text.
 * @param field Human-readable field name.
 */
void validate_relative_path(const std::string& value, const std::string_view field) {
    validate_text(value, field);
    if (value.front() == '/' || value.back() == '/' || value.find("//") != std::string::npos
        || value.find('\\') != std::string::npos || value.find(':') != std::string::npos) {
        invalid(std::string(field) + " is not a portable relative path");
    }
    std::size_t begin = 0;
    while (begin < value.size()) {
        const std::size_t separator = value.find('/', begin);
        const std::size_t end = separator == std::string::npos ? value.size() : separator;
        const std::string_view component(value.data() + begin, end - begin);
        if (component == "." || component == "..") {
            invalid(std::string(field) + " contains a forbidden path component");
        }
        if (separator == std::string::npos) {
            break;
        }
        begin = separator + 1U;
    }
}

/**
 * @brief Validates the fixed-width UTC acquisition timestamp.
 * @param value Timestamp text.
 */
void validate_timestamp(const std::string& value) {
    validate_text(value, "Acquisition timestamp");
    if (value.size() != 20U || value[4] != '-' || value[7] != '-'
        || value[10] != 'T' || value[13] != ':' || value[16] != ':'
        || value[19] != 'Z') {
        invalid("Acquisition timestamp must use YYYY-MM-DDTHH:MM:SSZ");
    }
    constexpr std::array<std::size_t, 6> separators{4, 7, 10, 13, 16, 19};
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (std::find(separators.begin(), separators.end(), index) == separators.end()
            && (value[index] < '0' || value[index] > '9')) {
            invalid("Acquisition timestamp must use YYYY-MM-DDTHH:MM:SSZ");
        }
    }
    const auto two_digits = [&value](const std::size_t offset) noexcept {
        return static_cast<unsigned int>(value[offset] - '0') * 10U
            + static_cast<unsigned int>(value[offset + 1U] - '0');
    };
    const unsigned int year = two_digits(0) * 100U + two_digits(2);
    const unsigned int month = two_digits(5);
    const unsigned int day = two_digits(8);
    const unsigned int hour = two_digits(11);
    const unsigned int minute = two_digits(14);
    const unsigned int second = two_digits(17);
    constexpr std::array<unsigned int, 12> days_by_month{
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (month == 0U || month > days_by_month.size()) {
        invalid("Acquisition timestamp contains an invalid month");
    }
    unsigned int maximum_day = days_by_month[month - 1U];
    const bool leap_year = (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
    if (month == 2U && leap_year) {
        maximum_day = 29U;
    }
    if (day == 0U || day > maximum_day || hour > 23U || minute > 59U || second > 59U) {
        invalid("Acquisition timestamp contains an invalid calendar value");
    }
}

/**
 * @brief Validates one derived tile-axis maximum.
 * @param origin Axis origin.
 * @param spacing Positive axis spacing.
 * @param dimension Axis sample count.
 */
void validate_bound(
    const double origin,
    const double spacing,
    const std::uint32_t dimension) {
    const double maximum = origin
        + spacing * static_cast<double>(static_cast<std::uint64_t>(dimension) - 1U);
    if (!std::isfinite(maximum)) {
        invalid("Derived terrain bound is not finite");
    }
}

/**
 * @brief Validates every logical value required by the offline encoder.
 * @param package Complete compiler-created package model.
 */
void validate_package_for_encoding(const WorldPackage& package) {
    validate_fixed_stage_semantics(package.provenance);
    if (package.header.magic != world_file_magic
        || package.header.major_version != current_major_version
        || package.header.minor_version != current_minor_version) {
        invalid("Compiler package header must use the current format version");
    }
    if (package.crs_epsg != staging_crs_epsg) {
        invalid("Compiler package CRS must be EPSG:5514");
    }

    const TerrainTile& terrain = package.terrain;
    if (terrain.columns == 0U || terrain.rows == 0U
        || terrain.columns > maximum_tile_dimension || terrain.rows > maximum_tile_dimension) {
        invalid("Terrain dimensions are outside the supported bounds");
    }
    const std::uint64_t samples = static_cast<std::uint64_t>(terrain.columns)
        * static_cast<std::uint64_t>(terrain.rows);
    if (samples > maximum_tile_samples || terrain.heights.size() != samples
        || terrain.validity.size() != samples) {
        invalid("Terrain vectors do not match bounded declared dimensions");
    }
    if (!std::isfinite(terrain.origin_x) || !std::isfinite(terrain.origin_y)
        || !std::isfinite(terrain.origin_z) || !std::isfinite(terrain.spacing_x)
        || !std::isfinite(terrain.spacing_y) || terrain.spacing_x <= 0.0
        || terrain.spacing_y <= 0.0) {
        invalid("Terrain world coordinates or spacing are invalid");
    }
    validate_bound(terrain.origin_x, terrain.spacing_x, terrain.columns);
    validate_bound(terrain.origin_y, terrain.spacing_y, terrain.rows);
    if (!std::all_of(terrain.heights.begin(), terrain.heights.end(), [](const float value) {
            return std::isfinite(value);
        })) {
        invalid("Terrain height contains a non-finite value");
    }
    if (!std::all_of(terrain.validity.begin(), terrain.validity.end(), [](const std::uint8_t value) {
            return value <= 1U;
        })) {
        invalid("Terrain validity byte is not zero or one");
    }
    validate_identifier(terrain.layer_id, "Layer ID");
    validate_identifier(terrain.tile_id, "Tile ID");

    const Provenance& provenance = package.provenance;
    for (const auto& [value, field] : std::array{
             std::pair{&provenance.provider, "Provider"},
             std::pair{&provenance.product, "Product"},
             std::pair{&provenance.edition, "Edition"},
             std::pair{&provenance.source_uri, "Source URI"},
             std::pair{&provenance.licence_name, "Licence name"},
             std::pair{&provenance.licence_uri, "Licence URI"},
             std::pair{&provenance.attribution, "Attribution"},
             std::pair{&provenance.rights_statement, "Rights statement"},
             std::pair{&provenance.source_classification, "Source classification"},
             std::pair{&provenance.stage_classification, "Stage classification"},
             std::pair{&provenance.source_crs, "Source CRS"},
             std::pair{&provenance.source_axis_order, "Source axis order"},
             std::pair{&provenance.source_horizontal_units, "Source horizontal units"},
             std::pair{&provenance.source_vertical_reference, "Source vertical reference"},
             std::pair{&provenance.source_vertical_units, "Source vertical units"},
             std::pair{&provenance.source_no_data_convention, "Source no-data convention"},
             std::pair{&provenance.stage_axis_order, "Stage axis order"},
             std::pair{&provenance.stage_horizontal_units, "Stage horizontal units"},
             std::pair{&provenance.stage_vertical_reference, "Stage vertical reference"},
             std::pair{&provenance.stage_vertical_units, "Stage vertical units"},
             std::pair{&provenance.stage_no_data_convention, "Stage no-data convention"},
             std::pair{&provenance.resolution, "Resolution"},
             std::pair{&provenance.bounds, "Bounds"},
             std::pair{&provenance.epoch, "Epoch"},
             std::pair{&provenance.transformation, "Transformation"},
             std::pair{&provenance.responsible_owner, "Responsible owner"}}) {
        validate_text(*value, field);
    }
    validate_timestamp(provenance.acquisition_timestamp);
    validate_source_filename(provenance.source_filename);
    validate_sha256(provenance.source_sha256, "Source SHA-256");
    if (provenance.staged_heights_byte_size > maximum_package_bytes
        || provenance.staged_validity_byte_size > maximum_package_bytes) {
        invalid("Staged-input byte size exceeds the supported bound");
    }
    validate_relative_path(provenance.staged_heights_path, "Staged heights path");
    validate_sha256(provenance.staged_heights_sha256, "Staged heights SHA-256");
    validate_relative_path(provenance.staged_validity_path, "Staged validity path");
    validate_sha256(provenance.staged_validity_sha256, "Staged validity SHA-256");
}

/**
 * @brief Adds two encoded sizes with overflow detection.
 * @param left First size.
 * @param right Second size.
 * @return Exact sum.
 */
[[nodiscard]] std::uint64_t checked_add(
    const std::uint64_t left,
    const std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        invalid("Package size addition overflow");
    }
    return left + right;
}

/**
 * @brief Multiplies two encoded sizes with overflow detection.
 * @param left First factor.
 * @param right Second factor.
 * @return Exact product.
 */
[[nodiscard]] std::uint64_t checked_multiply(
    const std::uint64_t left,
    const std::uint64_t right) {
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) {
        invalid("Package size multiplication overflow");
    }
    return left * right;
}

/**
 * @brief Appends one little-endian unsigned 16-bit integer.
 * @param bytes Destination byte vector.
 * @param value Value to encode.
 */
void append_u16(std::vector<std::uint8_t>& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

/**
 * @brief Appends one little-endian unsigned 32-bit integer.
 * @param bytes Destination byte vector.
 * @param value Value to encode.
 */
void append_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

/**
 * @brief Appends one little-endian unsigned 64-bit integer.
 * @param bytes Destination byte vector.
 * @param value Value to encode.
 */
void append_u64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
    for (unsigned int shift = 0; shift < 64U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFULL));
    }
}

/**
 * @brief Appends an IEEE-754 binary32 value in little-endian order.
 * @param bytes Destination byte vector.
 * @param value Value to encode.
 */
void append_f32(std::vector<std::uint8_t>& bytes, const float value) {
    static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == sizeof(std::uint32_t));
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

/**
 * @brief Appends an IEEE-754 binary64 value in little-endian order.
 * @param bytes Destination byte vector.
 * @param value Value to encode.
 */
void append_f64(std::vector<std::uint8_t>& bytes, const double value) {
    static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == sizeof(std::uint64_t));
    append_u64(bytes, std::bit_cast<std::uint64_t>(value));
}

/**
 * @brief Appends one little-endian-length-prefixed UTF-8 string.
 * @param bytes Destination byte vector.
 * @param value Already validated string.
 */
void append_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
    append_u32(bytes, static_cast<std::uint32_t>(value.size()));
    for (const char byte : value) {
        bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
}

/**
 * @brief Replaces one four-byte field with a little-endian integer.
 * @param bytes Complete mutable package bytes.
 * @param offset Destination byte offset.
 * @param value Value to encode.
 */
void patch_u32(
    std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::uint32_t value) noexcept {
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        bytes[offset + (shift / 8U)]
            = static_cast<std::uint8_t>((value >> shift) & 0xFFU);
    }
}

} // namespace

std::vector<std::uint8_t> encode_world_package(const WorldPackage& package) {
    validate_package_for_encoding(package);

    const TerrainTile& terrain = package.terrain;
    const Provenance& provenance = package.provenance;
    const std::uint64_t samples = static_cast<std::uint64_t>(terrain.columns)
        * static_cast<std::uint64_t>(terrain.rows);
    const std::array<const std::string*, metadata_string_count> metadata_strings{
        &terrain.layer_id,
        &terrain.tile_id,
        &provenance.provider,
        &provenance.product,
        &provenance.edition,
        &provenance.source_uri,
        &provenance.acquisition_timestamp,
        &provenance.licence_name,
        &provenance.licence_uri,
        &provenance.attribution,
        &provenance.rights_statement,
        &provenance.source_filename,
        &provenance.source_sha256,
        &provenance.source_classification,
        &provenance.stage_classification,
        &provenance.source_crs,
        &provenance.source_axis_order,
        &provenance.source_horizontal_units,
        &provenance.source_vertical_reference,
        &provenance.source_vertical_units,
        &provenance.source_no_data_convention,
        &provenance.stage_axis_order,
        &provenance.stage_horizontal_units,
        &provenance.stage_vertical_reference,
        &provenance.stage_vertical_units,
        &provenance.stage_no_data_convention,
        &provenance.resolution,
        &provenance.bounds,
        &provenance.epoch,
        &provenance.transformation,
        &provenance.responsible_owner,
        &provenance.staged_heights_path,
        &provenance.staged_heights_sha256,
        &provenance.staged_validity_path,
        &provenance.staged_validity_sha256};

    std::uint64_t metadata_size = 3U * sizeof(std::uint64_t);
    for (const std::string* const value : metadata_strings) {
        metadata_size = checked_add(metadata_size, sizeof(std::uint32_t));
        metadata_size = checked_add(metadata_size, value->size());
    }
    const std::uint64_t heights_offset = encoded_header_size;
    const std::uint64_t validity_offset = checked_add(
        heights_offset, checked_multiply(samples, sizeof(float)));
    const std::uint64_t metadata_offset = checked_add(validity_offset, samples);
    const std::uint64_t file_size = checked_add(metadata_offset, metadata_size);
    if (file_size > maximum_package_bytes) {
        invalid("Encoded package exceeds the package-size bound");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(file_size));
    for (const char byte : package.header.magic) {
        bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    append_u16(bytes, package.header.major_version);
    append_u16(bytes, package.header.minor_version);
    append_u32(bytes, encoded_header_size);
    append_u64(bytes, file_size);
    append_u32(bytes, 0U);
    append_u32(bytes, 0U);
    append_u32(bytes, package.crs_epsg);
    append_u32(bytes, terrain.columns);
    append_u32(bytes, terrain.rows);
    append_u32(bytes, metadata_string_count);
    append_f64(bytes, terrain.origin_x);
    append_f64(bytes, terrain.origin_y);
    append_f64(bytes, terrain.spacing_x);
    append_f64(bytes, terrain.spacing_y);
    append_u64(bytes, heights_offset);
    append_u64(bytes, validity_offset);
    append_u64(bytes, metadata_offset);
    append_u64(bytes, metadata_size);
    append_u64(bytes, samples);
    append_f64(bytes, terrain.origin_z);
    for (const float height : terrain.heights) {
        append_f32(bytes, height);
    }
    bytes.insert(bytes.end(), terrain.validity.begin(), terrain.validity.end());
    append_u64(bytes, provenance.source_byte_size);
    append_u64(bytes, provenance.staged_heights_byte_size);
    append_u64(bytes, provenance.staged_validity_byte_size);
    for (const std::string* const value : metadata_strings) {
        append_string(bytes, *value);
    }
    if (bytes.size() != file_size) {
        invalid("Internal package size calculation mismatch");
    }

    patch_u32(bytes, checksum_offset, crc32_iso_hdlc(bytes));
    return bytes;
}

} // namespace game_ex::world_format::detail
