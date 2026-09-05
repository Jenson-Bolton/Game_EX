/**
 * @file world_package.cpp
 * @brief Explicit little-endian package encoding and verified runtime decoding.
 */

#include "game_ex/world_format/world_package.hpp"

#include "world_format/package_layout.hpp"
#include "world_format/package_validation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace game_ex::world_format {
namespace {

/** Byte offset immediately after the fixed header flags. */
constexpr std::size_t payload_metadata_offset = 32;

/** Maximum byte value represented by a canonical validity entry. */
constexpr std::uint8_t maximum_validity_value = 1;

/** CRC-32/ISO-HDLC reversed polynomial. */
constexpr std::uint32_t crc32_polynomial = 0xEDB88320U;

/**
 * @brief Throws a classified format error.
 * @param message Human-readable invariant failure.
 */
[[noreturn]] void invalid_format(const std::string& message) {
    throw PackageError(PackageErrorCode::invalid_format, message);
}

/**
 * @brief Adds two unsigned sizes with overflow detection.
 * @param left First size.
 * @param right Second size.
 * @return Exact sum.
 */
[[nodiscard]] std::uint64_t checked_add(
    const std::uint64_t left,
    const std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        invalid_format("Package size addition overflow");
    }
    return left + right;
}

/**
 * @brief Multiplies two unsigned sizes with overflow detection.
 * @param left First factor.
 * @param right Second factor.
 * @return Exact product.
 */
[[nodiscard]] std::uint64_t checked_multiply(
    const std::uint64_t left,
    const std::uint64_t right) {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        invalid_format("Package size multiplication overflow");
    }
    return left * right;
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
 * @brief Validates one metadata string's bounded portable byte content.
 * @param value String to inspect.
 * @param field Human-readable field name.
 */
void validate_metadata_string(const std::string& value, const std::string_view field) {
    if (value.empty()) {
        invalid_format(std::string(field) + " cannot be empty");
    }
    if (value.size() > maximum_metadata_string_bytes) {
        invalid_format(std::string(field) + " exceeds the metadata byte bound");
    }
    if (!is_valid_utf8(value)) {
        invalid_format(std::string(field) + " is not valid UTF-8");
    }
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte == 0x7FU) {
            invalid_format(std::string(field) + " contains a control byte");
        }
    }
}

/**
 * @brief Validates one stable ASCII layer or tile identifier.
 * @param value Identifier to inspect.
 * @param field Human-readable field name.
 */
void validate_identifier(const std::string& value, const std::string_view field) {
    validate_metadata_string(value, field);
    if (value.size() > 64) {
        invalid_format(std::string(field) + " exceeds 64 bytes");
    }
    const bool valid = std::all_of(value.begin(), value.end(), [](const char character) {
        const auto byte = static_cast<unsigned char>(character);
        return (byte >= static_cast<unsigned char>('a')
                && byte <= static_cast<unsigned char>('z'))
            || (byte >= static_cast<unsigned char>('A')
                && byte <= static_cast<unsigned char>('Z'))
            || (byte >= static_cast<unsigned char>('0')
                && byte <= static_cast<unsigned char>('9'))
            || byte == static_cast<unsigned char>('_')
            || byte == static_cast<unsigned char>('-')
            || byte == static_cast<unsigned char>('.');
    });
    if (!valid) {
        invalid_format(std::string(field) + " contains a non-portable identifier byte");
    }
}

/**
 * @brief Validates the canonical lowercase hexadecimal SHA-256 spelling.
 * @param value Digest text to inspect.
 * @param field Human-readable field name.
 */
void validate_sha256(const std::string& value, const std::string_view field) {
    if (value.size() != 64) {
        invalid_format(std::string(field)
            + " must contain exactly 64 lowercase hexadecimal digits");
    }
    const bool valid = std::all_of(value.begin(), value.end(), [](const char byte) {
        return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
    });
    if (!valid) {
        invalid_format(std::string(field)
            + " must contain exactly 64 lowercase hexadecimal digits");
    }
}

/**
 * @brief Validates the fixed UTC acquisition timestamp spelling.
 * @param value Timestamp text to inspect.
 */
void validate_acquisition_timestamp(const std::string& value) {
    validate_metadata_string(value, "Acquisition timestamp");
    if (value.size() != 20 || value[4] != '-' || value[7] != '-'
        || value[10] != 'T' || value[13] != ':' || value[16] != ':'
        || value[19] != 'Z') {
        invalid_format("Acquisition timestamp must use YYYY-MM-DDTHH:MM:SSZ");
    }
    constexpr std::array<std::size_t, 6> separators{4, 7, 10, 13, 16, 19};
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (std::find(separators.begin(), separators.end(), index) == separators.end()
            && (value[index] < '0' || value[index] > '9')) {
            invalid_format("Acquisition timestamp must use YYYY-MM-DDTHH:MM:SSZ");
        }
    }
    const auto two_digits = [&value](const std::size_t offset) noexcept {
        return static_cast<unsigned int>(value[offset] - '0') * 10U
            + static_cast<unsigned int>(value[offset + 1] - '0');
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
        invalid_format("Acquisition timestamp contains an invalid month");
    }
    unsigned int maximum_day = days_by_month[month - 1U];
    const bool leap_year = (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
    if (month == 2U && leap_year) {
        maximum_day = 29U;
    }
    if (day == 0U || day > maximum_day || hour > 23U || minute > 59U || second > 59U) {
        invalid_format("Acquisition timestamp contains an invalid calendar value");
    }
}

/**
 * @brief Validates the restricted original source name without path semantics.
 * @param value Source filename to inspect.
 */
void validate_source_filename(const std::string& value) {
    validate_metadata_string(value, "Source filename");
    if (value == "." || value == ".." || value.find('/') != std::string::npos
        || value.find('\\') != std::string::npos || value.find(':') != std::string::npos) {
        invalid_format("Source filename must be a restricted path-free name");
    }
    const bool valid = std::all_of(value.begin(), value.end(), [](const char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9') || byte == '.' || byte == '_'
            || byte == '-';
    });
    if (!valid) {
        invalid_format("Source filename contains a byte outside the restricted name alphabet");
    }
}

/**
 * @brief Validates a portable manifest-relative staged path lexically.
 * @param value Path text to inspect without accessing the filesystem.
 * @param field Human-readable field name.
 */
void validate_relative_path(const std::string& value, const std::string_view field) {
    validate_metadata_string(value, field);
    if (value.front() == '/' || value.back() == '/' || value.find("//") != std::string::npos
        || value.find('\\') != std::string::npos || value.find(':') != std::string::npos) {
        invalid_format(std::string(field) + " is not a portable relative path");
    }
    std::size_t begin = 0;
    while (begin < value.size()) {
        const std::size_t separator = value.find('/', begin);
        const std::size_t end = separator == std::string::npos ? value.size() : separator;
        const std::string_view component(value.data() + begin, end - begin);
        if (component == "." || component == "..") {
            invalid_format(std::string(field) + " contains a forbidden path component");
        }
        if (separator == std::string::npos) {
            break;
        }
        begin = separator + 1U;
    }
}

/**
 * @brief Computes the canonical tile sample count and enforces bounds.
 * @param columns Tile column count.
 * @param rows Tile row count.
 * @return Exact bounded sample count.
 */
[[nodiscard]] std::uint64_t validated_sample_count(
    const std::uint32_t columns,
    const std::uint32_t rows) {
    if (columns == 0 || rows == 0
        || columns > maximum_tile_dimension || rows > maximum_tile_dimension) {
        invalid_format("Terrain dimensions must be between 1 and 4096");
    }
    const std::uint64_t samples = checked_multiply(columns, rows);
    if (samples > maximum_tile_samples) {
        invalid_format("Terrain sample count exceeds the staging bound");
    }
    return samples;
}

/**
 * @brief Validates one derived positive-axis tile bound.
 * @param origin Double-precision first-sample coordinate.
 * @param spacing Positive sample spacing.
 * @param dimension Sample count along the axis.
 */
void validate_derived_bound(
    const double origin,
    const double spacing,
    const std::uint32_t dimension) {
    const double maximum = origin
        + spacing * static_cast<double>(static_cast<std::uint64_t>(dimension) - 1U);
    if (!std::isfinite(maximum)) {
        invalid_format("Derived terrain bound is not finite");
    }
}

/**
 * @brief Computes CRC-32 while treating the stored checksum field as zero.
 * @param bytes Complete package bytes.
 * @return Canonical CRC-32 value.
 */
[[nodiscard]] std::uint32_t package_crc32(const std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const bool checksum_byte = index >= detail::checksum_offset
            && index < detail::checksum_offset + sizeof(std::uint32_t);
        const std::uint8_t value = checksum_byte ? std::uint8_t{0} : bytes[index];
        crc ^= value;
        for (unsigned int bit = 0; bit < 8U; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (crc32_polynomial & mask);
        }
    }
    return ~crc;
}

/**
 * @brief Bounds-checking random-access little-endian decoder.
 */
class Decoder final {
public:
    /**
     * @brief Binds a decoder to immutable complete package bytes.
     * @param bytes Package storage that outlives this decoder.
     */
    explicit Decoder(const std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    /**
     * @brief Reads an unsigned 16-bit value.
     * @param offset Byte offset.
     * @return Decoded value.
     */
    [[nodiscard]] std::uint16_t u16(const std::size_t offset) const {
        require_range(offset, sizeof(std::uint16_t));
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes_[offset])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes_[offset + 1]) << 8U));
    }

    /**
     * @brief Reads an unsigned 32-bit value.
     * @param offset Byte offset.
     * @return Decoded value.
     */
    [[nodiscard]] std::uint32_t u32(const std::size_t offset) const {
        require_range(offset, sizeof(std::uint32_t));
        std::uint32_t value = 0;
        for (unsigned int shift = 0; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset + (shift / 8U)]) << shift;
        }
        return value;
    }

    /**
     * @brief Reads an unsigned 64-bit value.
     * @param offset Byte offset.
     * @return Decoded value.
     */
    [[nodiscard]] std::uint64_t u64(const std::size_t offset) const {
        require_range(offset, sizeof(std::uint64_t));
        std::uint64_t value = 0;
        for (unsigned int shift = 0; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset + (shift / 8U)]) << shift;
        }
        return value;
    }

    /**
     * @brief Reads an IEEE-754 binary32 value.
     * @param offset Byte offset.
     * @return Decoded floating-point value.
     */
    [[nodiscard]] float f32(const std::size_t offset) const {
        return std::bit_cast<float>(u32(offset));
    }

    /**
     * @brief Reads an IEEE-754 binary64 value.
     * @param offset Byte offset.
     * @return Decoded floating-point value.
     */
    [[nodiscard]] double f64(const std::size_t offset) const {
        return std::bit_cast<double>(u64(offset));
    }

    /**
     * @brief Reads one bounded length-prefixed string and advances a cursor.
     * @param cursor Mutable byte position.
     * @param limit Exclusive metadata-block end.
     * @param field Human-readable field name.
     * @return Validated string bytes.
     */
    [[nodiscard]] std::string string(
        std::size_t& cursor,
        const std::size_t limit,
        const std::string_view field) const {
        if (cursor > limit || limit > bytes_.size() || limit - cursor < sizeof(std::uint32_t)) {
            invalid_format("Metadata block is truncated");
        }
        const std::uint32_t encoded_size = u32(cursor);
        cursor += sizeof(std::uint32_t);
        if (encoded_size > maximum_metadata_string_bytes
            || static_cast<std::size_t>(encoded_size) > limit - cursor) {
            invalid_format(std::string(field) + " length is outside the metadata block");
        }
        const auto* const begin = reinterpret_cast<const char*>(bytes_.data() + cursor);
        std::string value(begin, static_cast<std::size_t>(encoded_size));
        cursor += encoded_size;
        validate_metadata_string(value, field);
        return value;
    }

private:
    /**
     * @brief Rejects an offset-length pair outside package storage.
     * @param offset First byte offset.
     * @param length Required byte count.
     */
    void require_range(const std::size_t offset, const std::size_t length) const {
        if (offset > bytes_.size() || length > bytes_.size() - offset) {
            invalid_format("Package field extends beyond the file");
        }
    }

    /** Immutable package bytes owned by the reader call. */
    std::span<const std::uint8_t> bytes_;
};

/**
 * @brief Reads one bounded package file into memory.
 * @param path Package path.
 * @return Complete bytes matching the filesystem size.
 */
[[nodiscard]] std::vector<std::uint8_t> read_package_bytes(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, size_error);
    if (size_error) {
        throw PackageError(PackageErrorCode::io, "Cannot determine package size: " + path.string());
    }
    if (file_size < detail::encoded_header_size || file_size > maximum_package_bytes) {
        invalid_format("Package size is outside the supported bounds");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw PackageError(PackageErrorCode::io, "Cannot open package: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) || !input
        || input.peek() != std::char_traits<char>::eof()) {
        throw PackageError(PackageErrorCode::io, "Could not read complete package: " + path.string());
    }
    return bytes;
}

} // namespace

PackageError::PackageError(const PackageErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

PackageErrorCode PackageError::code() const noexcept {
    return code_;
}

WorldPackage read_world_package(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_package_bytes(path);
    const Decoder decoder(bytes);

    const std::uint32_t stored_checksum = decoder.u32(detail::checksum_offset);
    if (package_crc32(bytes) != stored_checksum) {
        throw PackageError(PackageErrorCode::integrity, "Package CRC-32 integrity check failed");
    }

    WorldHeader header{};
    for (std::size_t index = 0; index < header.magic.size(); ++index) {
        header.magic[index] = static_cast<char>(bytes[index]);
    }
    header.major_version = decoder.u16(8);
    header.minor_version = decoder.u16(10);
    if (header.magic != world_file_magic) {
        invalid_format("Package magic does not identify Game_EX world data");
    }
    if (header.major_version != current_major_version
        || header.minor_version != current_minor_version) {
        throw PackageError(PackageErrorCode::unsupported_version, "Unsupported world-package version");
    }

    if (decoder.u32(12) != detail::encoded_header_size) {
        invalid_format("Package header size is not supported");
    }
    if (decoder.u64(16) != bytes.size()) {
        invalid_format("Encoded package size does not match the file");
    }
    if (decoder.u32(28) != 0) {
        invalid_format("Package flags contain unsupported bits");
    }

    const std::uint32_t crs_epsg = decoder.u32(payload_metadata_offset);
    const std::uint32_t columns = decoder.u32(36);
    const std::uint32_t rows = decoder.u32(40);
    if (crs_epsg != staging_crs_epsg) {
        invalid_format("Package CRS must be EPSG:5514");
    }
    if (decoder.u32(44) != detail::metadata_string_count) {
        invalid_format("Package metadata string count is not supported");
    }

    const double origin_x = decoder.f64(48);
    const double origin_y = decoder.f64(56);
    const double spacing_x = decoder.f64(64);
    const double spacing_y = decoder.f64(72);
    const double origin_z = decoder.f64(120);
    if (!std::isfinite(origin_x) || !std::isfinite(origin_y)
        || !std::isfinite(origin_z) || !std::isfinite(spacing_x) || !std::isfinite(spacing_y)
        || spacing_x <= 0.0 || spacing_y <= 0.0) {
        invalid_format("Package world coordinates or spacing are invalid");
    }
    const std::uint64_t samples = validated_sample_count(columns, rows);
    validate_derived_bound(origin_x, spacing_x, columns);
    validate_derived_bound(origin_y, spacing_y, rows);
    if (decoder.u64(112) != samples) {
        invalid_format("Encoded sample count does not match terrain dimensions");
    }
    const std::uint64_t heights_offset = decoder.u64(80);
    const std::uint64_t validity_offset = decoder.u64(88);
    const std::uint64_t metadata_offset = decoder.u64(96);
    const std::uint64_t metadata_size = decoder.u64(104);
    const std::uint64_t expected_validity = checked_add(
        detail::encoded_header_size, checked_multiply(samples, sizeof(float)));
    const std::uint64_t expected_metadata = checked_add(expected_validity, samples);
    const std::uint64_t expected_file_size = checked_add(expected_metadata, metadata_size);
    if (heights_offset != detail::encoded_header_size
        || validity_offset != expected_validity
        || metadata_offset != expected_metadata
        || expected_file_size != bytes.size()) {
        invalid_format("Package section offsets are not canonical");
    }

    TerrainTile terrain{};
    terrain.columns = columns;
    terrain.rows = rows;
    terrain.origin_x = origin_x;
    terrain.origin_y = origin_y;
    terrain.origin_z = origin_z;
    terrain.spacing_x = spacing_x;
    terrain.spacing_y = spacing_y;
    terrain.heights.reserve(static_cast<std::size_t>(samples));
    for (std::uint64_t index = 0; index < samples; ++index) {
        const std::uint64_t byte_offset = checked_add(heights_offset, index * sizeof(float));
        const float height = decoder.f32(static_cast<std::size_t>(byte_offset));
        if (!std::isfinite(height)) {
            invalid_format("Terrain height contains a non-finite value");
        }
        terrain.heights.push_back(height);
    }

    terrain.validity.reserve(static_cast<std::size_t>(samples));
    for (std::uint64_t index = 0; index < samples; ++index) {
        const std::uint8_t value = bytes[static_cast<std::size_t>(validity_offset + index)];
        if (value > maximum_validity_value) {
            invalid_format("Terrain validity byte is not zero or one");
        }
        terrain.validity.push_back(value);
    }

    std::size_t metadata_cursor = static_cast<std::size_t>(metadata_offset);
    const std::size_t metadata_end = static_cast<std::size_t>(expected_file_size);
    Provenance provenance{};
    constexpr std::size_t metadata_numeric_bytes = 3 * sizeof(std::uint64_t);
    if (metadata_cursor > metadata_end
        || metadata_end - metadata_cursor < metadata_numeric_bytes) {
        invalid_format("Metadata block is truncated before staged-input sizes");
    }
    provenance.source_byte_size = decoder.u64(metadata_cursor);
    metadata_cursor += sizeof(std::uint64_t);
    provenance.staged_heights_byte_size = decoder.u64(metadata_cursor);
    metadata_cursor += sizeof(std::uint64_t);
    provenance.staged_validity_byte_size = decoder.u64(metadata_cursor);
    metadata_cursor += sizeof(std::uint64_t);
    if (provenance.staged_heights_byte_size > maximum_package_bytes
        || provenance.staged_validity_byte_size > maximum_package_bytes) {
        invalid_format("Staged-input byte size exceeds the supported bound");
    }
    terrain.layer_id = decoder.string(metadata_cursor, metadata_end, "Layer ID");
    terrain.tile_id = decoder.string(metadata_cursor, metadata_end, "Tile ID");
    validate_identifier(terrain.layer_id, "Layer ID");
    validate_identifier(terrain.tile_id, "Tile ID");

    provenance.provider = decoder.string(metadata_cursor, metadata_end, "Provider");
    provenance.product = decoder.string(metadata_cursor, metadata_end, "Product");
    provenance.edition = decoder.string(metadata_cursor, metadata_end, "Edition");
    provenance.source_uri = decoder.string(metadata_cursor, metadata_end, "Source URI");
    provenance.acquisition_timestamp = decoder.string(
        metadata_cursor, metadata_end, "Acquisition timestamp");
    provenance.licence_name = decoder.string(metadata_cursor, metadata_end, "Licence name");
    provenance.licence_uri = decoder.string(metadata_cursor, metadata_end, "Licence URI");
    provenance.attribution = decoder.string(metadata_cursor, metadata_end, "Attribution");
    provenance.rights_statement = decoder.string(
        metadata_cursor, metadata_end, "Rights statement");
    provenance.source_filename = decoder.string(
        metadata_cursor, metadata_end, "Source filename");
    provenance.source_sha256 = decoder.string(metadata_cursor, metadata_end, "Source SHA-256");
    provenance.source_classification = decoder.string(
        metadata_cursor, metadata_end, "Source classification");
    provenance.stage_classification = decoder.string(
        metadata_cursor, metadata_end, "Stage classification");
    provenance.source_crs = decoder.string(metadata_cursor, metadata_end, "Source CRS");
    provenance.source_axis_order = decoder.string(
        metadata_cursor, metadata_end, "Source axis order");
    provenance.source_horizontal_units = decoder.string(
        metadata_cursor, metadata_end, "Source horizontal units");
    provenance.source_vertical_reference = decoder.string(
        metadata_cursor, metadata_end, "Source vertical reference");
    provenance.source_vertical_units = decoder.string(
        metadata_cursor, metadata_end, "Source vertical units");
    provenance.source_no_data_convention = decoder.string(
        metadata_cursor, metadata_end, "Source no-data convention");
    provenance.stage_axis_order = decoder.string(
        metadata_cursor, metadata_end, "Stage axis order");
    provenance.stage_horizontal_units = decoder.string(
        metadata_cursor, metadata_end, "Stage horizontal units");
    provenance.stage_vertical_reference = decoder.string(
        metadata_cursor, metadata_end, "Stage vertical reference");
    provenance.stage_vertical_units = decoder.string(
        metadata_cursor, metadata_end, "Stage vertical units");
    provenance.stage_no_data_convention = decoder.string(
        metadata_cursor, metadata_end, "Stage no-data convention");
    provenance.resolution = decoder.string(metadata_cursor, metadata_end, "Resolution");
    provenance.bounds = decoder.string(metadata_cursor, metadata_end, "Bounds");
    provenance.epoch = decoder.string(metadata_cursor, metadata_end, "Epoch");
    provenance.transformation = decoder.string(
        metadata_cursor, metadata_end, "Transformation");
    provenance.responsible_owner = decoder.string(
        metadata_cursor, metadata_end, "Responsible owner");
    provenance.staged_heights_path = decoder.string(
        metadata_cursor, metadata_end, "Staged heights path");
    provenance.staged_heights_sha256 = decoder.string(
        metadata_cursor, metadata_end, "Staged heights SHA-256");
    provenance.staged_validity_path = decoder.string(
        metadata_cursor, metadata_end, "Staged validity path");
    provenance.staged_validity_sha256 = decoder.string(
        metadata_cursor, metadata_end, "Staged validity SHA-256");
    validate_acquisition_timestamp(provenance.acquisition_timestamp);
    validate_source_filename(provenance.source_filename);
    validate_sha256(provenance.source_sha256, "Source SHA-256");
    validate_relative_path(provenance.staged_heights_path, "Staged heights path");
    validate_sha256(provenance.staged_heights_sha256, "Staged heights SHA-256");
    validate_relative_path(provenance.staged_validity_path, "Staged validity path");
    validate_sha256(provenance.staged_validity_sha256, "Staged validity SHA-256");
    detail::validate_fixed_stage_semantics(provenance);
    if (metadata_cursor != metadata_end) {
        invalid_format("Metadata block contains trailing bytes");
    }

    return {header, crs_epsg, std::move(terrain), std::move(provenance), stored_checksum};
}

namespace detail {

std::uint32_t crc32_iso_hdlc(const std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t value : bytes) {
        crc ^= value;
        for (unsigned int bit = 0; bit < 8U; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (crc32_polynomial & mask);
        }
    }
    return ~crc;
}

} // namespace detail
} // namespace game_ex::world_format
