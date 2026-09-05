/**
 * @file world_compiler.cpp
 * @brief Strict staging-manifest loading and deterministic package writing.
 */

#include "game_ex/world_compiler/world_compiler.hpp"

#include "package_encoding.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace game_ex::world_compiler {
namespace {

/** Maximum accepted staging-manifest byte count. */
constexpr std::uintmax_t maximum_manifest_bytes = 64U * 1024U;

/** Maximum accepted byte count for one text sample file. */
constexpr std::uintmax_t maximum_staged_text_bytes =
    world_format::maximum_package_bytes;

/** Maximum accepted byte count for a portable relative staged path. */
constexpr std::size_t maximum_staged_path_bytes = 1024;

/** Exact required manifest keys for schema version one. */
constexpr std::array<std::string_view, 50> required_manifest_keys{
    "format",
    "version",
    "crs",
    "terrain.layer_count",
    "terrain.layer_id",
    "terrain.tile_count",
    "terrain.tile_id",
    "terrain.tile.columns",
    "terrain.tile.rows",
    "terrain.tile.origin_x",
    "terrain.tile.origin_y",
    "terrain.tile.origin_z",
    "terrain.tile.spacing_x",
    "terrain.tile.spacing_y",
    "terrain.tile.heights",
    "terrain.tile.validity",
    "provenance.provider",
    "provenance.product",
    "provenance.edition",
    "provenance.source_uri",
    "provenance.acquired_utc",
    "provenance.licence_name",
    "provenance.licence_uri",
    "provenance.attribution",
    "provenance.rights",
    "provenance.source_filename",
    "provenance.source_byte_size",
    "provenance.source_sha256",
    "provenance.source_classification",
    "provenance.stage_classification",
    "source.crs",
    "source.axis_order",
    "source.horizontal_units",
    "source.vertical_reference",
    "source.vertical_units",
    "source.no_data",
    "stage.axis_order",
    "stage.horizontal_units",
    "stage.vertical_reference",
    "stage.vertical_units",
    "stage.no_data",
    "terrain.resolution",
    "spatial.bounds",
    "spatial.epoch",
    "provenance.transformation",
    "provenance.responsible_owner",
    "staged.heights.byte_size",
    "staged.heights.sha256",
    "staged.validity.byte_size",
    "staged.validity.sha256"};

/** Parsed key/value fields keyed independently of source order. */
using ManifestFields = std::map<std::string, std::string, std::less<>>;

/**
 * @brief Throws a classified manifest error.
 * @param message Human-readable invariant failure.
 */
[[noreturn]] void invalid_manifest(const std::string& message) {
    throw CompilerError(CompilerErrorCode::invalid_manifest, message);
}

/**
 * @brief Throws a classified staged-input error.
 * @param message Human-readable invariant failure.
 */
[[noreturn]] void invalid_staged_input(const std::string& message) {
    throw CompilerError(CompilerErrorCode::invalid_staged_input, message);
}

/**
 * @brief Throws a classified compiler I/O error.
 * @param message Human-readable I/O diagnostic.
 */
[[noreturn]] void io_error(const std::string& message) {
    throw CompilerError(CompilerErrorCode::io, message);
}

/**
 * @brief Determines whether a byte is portable manifest whitespace.
 * @param value Byte to inspect.
 * @return True for ASCII space or horizontal tab.
 */
[[nodiscard]] constexpr bool is_horizontal_space(const char value) noexcept {
    return value == ' ' || value == '\t';
}

/**
 * @brief Determines whether a manifest key is part of schema version one.
 * @param key Candidate key.
 * @return True only for an exact required key.
 */
[[nodiscard]] bool is_required_key(const std::string_view key) noexcept {
    for (const std::string_view required : required_manifest_keys) {
        if (required == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Reads a bounded regular text file completely.
 * @param path File to read.
 * @param maximum_bytes Inclusive size bound.
 * @param description Stable file role used in diagnostics.
 * @param staged Whether failures belong to the staged-input category.
 * @return File bytes interpreted as a string without text translation.
 */
[[nodiscard]] std::string read_bounded_text_file(
    const std::filesystem::path& path,
    const std::uintmax_t maximum_bytes,
    const std::string_view description,
    const bool staged) {
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(path, error);
    if (error || !regular) {
        const std::string message = std::string(description) + " is not a readable regular file: "
            + path.string();
        if (staged) {
            invalid_staged_input(message);
        }
        io_error(message);
    }

    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        const std::string message = "Cannot determine " + std::string(description)
            + " size: " + path.string();
        if (staged) {
            invalid_staged_input(message);
        }
        io_error(message);
    }
    if (size > maximum_bytes) {
        const std::string message = std::string(description) + " exceeds its byte limit";
        if (staged) {
            invalid_staged_input(message);
        }
        invalid_manifest(message);
    }
    if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        const std::string message = std::string(description) + " cannot fit in memory";
        if (staged) {
            invalid_staged_input(message);
        }
        invalid_manifest(message);
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        const std::string message = "Cannot open " + std::string(description) + ": "
            + path.string();
        if (staged) {
            invalid_staged_input(message);
        }
        io_error(message);
    }

    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream || stream.peek() != std::char_traits<char>::eof()) {
        const std::string message = "Cannot completely read " + std::string(description)
            + ": " + path.string();
        if (staged) {
            invalid_staged_input(message);
        }
        io_error(message);
    }
    return bytes;
}

/**
 * @brief Parses the strict line-oriented manifest grammar.
 * @param bytes Complete manifest bytes.
 * @return Exact key/value map.
 */
[[nodiscard]] ManifestFields parse_manifest(const std::string_view bytes) {
    ManifestFields fields;
    std::size_t line_number = 1;
    std::size_t line_begin = 0;

    while (line_begin <= bytes.size()) {
        const std::size_t newline = bytes.find('\n', line_begin);
        const std::size_t line_end = newline == std::string_view::npos
            ? bytes.size()
            : newline;
        std::string_view line = bytes.substr(line_begin, line_end - line_begin);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (!line.empty() && line.front() != '#') {
            const std::size_t separator = line.find('=');
            if (separator == std::string_view::npos || separator == 0
                || separator + 1 >= line.size()) {
                invalid_manifest(
                    "Manifest line " + std::to_string(line_number)
                    + " must be a non-empty key=value pair");
            }

            const std::string_view key = line.substr(0, separator);
            const std::string_view value = line.substr(separator + 1);
            if (is_horizontal_space(key.front()) || is_horizontal_space(key.back())
                || is_horizontal_space(value.front()) || is_horizontal_space(value.back())) {
                invalid_manifest(
                    "Manifest line " + std::to_string(line_number)
                    + " has forbidden surrounding whitespace");
            }
            for (const char byte : key) {
                if (is_horizontal_space(byte) || byte == '\r' || byte == '\n') {
                    invalid_manifest(
                        "Manifest line " + std::to_string(line_number)
                        + " contains whitespace in its key");
                }
            }
            if (!is_required_key(key)) {
                invalid_manifest("Unknown manifest key: " + std::string(key));
            }
            const auto [unused, inserted] = fields.emplace(
                std::string(key), std::string(value));
            static_cast<void>(unused);
            if (!inserted) {
                invalid_manifest("Duplicate manifest key: " + std::string(key));
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        line_begin = newline + 1;
        ++line_number;
    }

    for (const std::string_view key : required_manifest_keys) {
        if (!fields.contains(key)) {
            invalid_manifest("Missing manifest key: " + std::string(key));
        }
    }
    return fields;
}

/**
 * @brief Returns one already-required manifest value.
 * @param fields Parsed manifest fields.
 * @param key Required key.
 * @return Stable reference to the value.
 */
[[nodiscard]] const std::string& field(
    const ManifestFields& fields,
    const std::string_view key) {
    return fields.at(std::string(key));
}

/**
 * @brief Requires an exact schema marker value.
 * @param fields Parsed manifest fields.
 * @param key Required key.
 * @param expected Required value.
 */
void require_exact(
    const ManifestFields& fields,
    const std::string_view key,
    const std::string_view expected) {
    if (field(fields, key) != expected) {
        invalid_manifest(
            "Manifest key " + std::string(key) + " must equal "
            + std::string(expected));
    }
}

/**
 * @brief Parses a canonical unsigned decimal manifest number.
 * @param value Complete numeric field value.
 * @param name Field name used in diagnostics.
 * @return Parsed 32-bit value.
 */
[[nodiscard]] std::uint32_t parse_u32(
    const std::string_view value,
    const std::string_view name) {
    std::uint32_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        invalid_manifest("Manifest key " + std::string(name)
            + " must be an unsigned decimal integer");
    }
    return parsed;
}

/**
 * @brief Parses a canonical unsigned 64-bit decimal manifest number.
 * @param value Complete numeric field value.
 * @param name Field name used in diagnostics.
 * @return Parsed 64-bit value.
 */
[[nodiscard]] std::uint64_t parse_u64(
    const std::string_view value,
    const std::string_view name) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        invalid_manifest("Manifest key " + std::string(name)
            + " must be an unsigned decimal integer");
    }
    return parsed;
}

/**
 * @brief Parses a finite decimal floating-point manifest number.
 * @param value Complete numeric field value.
 * @param name Field name used in diagnostics.
 * @return Parsed double-precision value.
 */
[[nodiscard]] double parse_double(
    const std::string_view value,
    const std::string_view name) {
    double parsed = 0.0;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()
        || !std::isfinite(parsed)) {
        invalid_manifest("Manifest key " + std::string(name)
            + " must be a finite decimal number");
    }
    return parsed;
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
 * @brief Validates a portable identifier or provenance value.
 * @param value Value to inspect.
 * @param name Manifest field name.
 * @param identifier Whether identifier character restrictions apply.
 */
void validate_text_field(
    const std::string& value,
    const std::string_view name,
    const bool identifier) {
    if (value.empty()) {
        invalid_manifest("Manifest key " + std::string(name) + " cannot be empty");
    }
    if (value.size() > world_format::maximum_metadata_string_bytes) {
        invalid_manifest("Manifest key " + std::string(name) + " exceeds its byte limit");
    }
    if (identifier && value.size() > 64U) {
        invalid_manifest("Manifest key " + std::string(name) + " exceeds 64 bytes");
    }
    if (!is_valid_utf8(value)) {
        invalid_manifest("Manifest key " + std::string(name) + " is not valid UTF-8");
    }
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte == 0x7FU) {
            invalid_manifest("Manifest key " + std::string(name)
                + " contains a control byte");
        }
        if (identifier) {
            const bool allowed = (byte >= 'a' && byte <= 'z')
                || (byte >= 'A' && byte <= 'Z')
                || (byte >= '0' && byte <= '9')
                || byte == '_' || byte == '-' || byte == '.';
            if (!allowed) {
                invalid_manifest("Manifest key " + std::string(name)
                    + " contains a non-portable identifier byte");
            }
        }
    }
}

/**
 * @brief Validates one mandatory canonical SHA-256 spelling.
 * @param value Digest text to inspect.
 * @param name Manifest field name used in diagnostics.
 */
void validate_sha256(const std::string& value, const std::string_view name) {
    if (value.size() != 64) {
        invalid_manifest("Manifest key " + std::string(name)
            + " must contain 64 lowercase hexadecimal digits");
    }
    for (const char byte : value) {
        if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'))) {
            invalid_manifest("Manifest key " + std::string(name)
                + " must contain 64 lowercase hexadecimal digits");
        }
    }
}

/**
 * @brief Validates the mandatory fixed-width UTC acquisition timestamp.
 * @param value Timestamp text to inspect.
 */
void validate_acquired_utc(const std::string& value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-'
        || value[10] != 'T' || value[13] != ':' || value[16] != ':'
        || value[19] != 'Z') {
        invalid_manifest(
            "Manifest key provenance.acquired_utc must use YYYY-MM-DDTHH:MM:SSZ");
    }
    constexpr std::array<std::size_t, 6> separators{4, 7, 10, 13, 16, 19};
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (std::find(separators.begin(), separators.end(), index) == separators.end()
            && (value[index] < '0' || value[index] > '9')) {
            invalid_manifest(
                "Manifest key provenance.acquired_utc must use YYYY-MM-DDTHH:MM:SSZ");
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
        invalid_manifest("Manifest acquisition timestamp contains an invalid month");
    }
    unsigned int maximum_day = days_by_month[month - 1U];
    const bool leap_year = (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
    if (month == 2U && leap_year) {
        maximum_day = 29U;
    }
    if (day == 0U || day > maximum_day || hour > 23U || minute > 59U || second > 59U) {
        invalid_manifest("Manifest acquisition timestamp contains an invalid calendar value");
    }
}

/**
 * @brief Validates the original source filename as a restricted path-free name.
 * @param value Filename text to inspect.
 */
void validate_source_filename(const std::string& value) {
    if (value == "." || value == ".." || value.find('/') != std::string::npos
        || value.find('\\') != std::string::npos || value.find(':') != std::string::npos) {
        invalid_manifest(
            "Manifest key provenance.source_filename must be a restricted path-free name");
    }
    const bool valid = std::all_of(value.begin(), value.end(), [](const char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9') || byte == '.' || byte == '_'
            || byte == '-';
    });
    if (!valid) {
        invalid_manifest(
            "Manifest key provenance.source_filename contains a byte outside the restricted alphabet");
    }
}

/** Round constants for SHA-256 compression. */
constexpr std::array<std::uint32_t, 64> sha256_round_constants{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U};

/**
 * @brief Computes canonical SHA-256 for exact staged-file bytes.
 * @param input Bytes to hash.
 * @return Sixty-four lowercase hexadecimal characters.
 */
[[nodiscard]] std::string sha256_hex(const std::string_view input) {
    std::vector<std::uint8_t> message;
    message.reserve(input.size() + 72U);
    for (const char byte : input) {
        message.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    message.push_back(0x80U);
    while (message.size() % 64U != 56U) {
        message.push_back(0U);
    }
    const std::uint64_t bit_count = static_cast<std::uint64_t>(input.size()) * 8U;
    for (std::size_t index = 0; index < sizeof(bit_count); ++index) {
        const unsigned int shift = static_cast<unsigned int>(56U - index * 8U);
        message.push_back(static_cast<std::uint8_t>((bit_count >> shift) & 0xFFU));
    }

    std::array<std::uint32_t, 8> state{
        0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
    for (std::size_t block = 0; block < message.size(); block += 64U) {
        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t index = 0; index < 16U; ++index) {
            const std::size_t offset = block + index * 4U;
            schedule[index] = (static_cast<std::uint32_t>(message[offset]) << 24U)
                | (static_cast<std::uint32_t>(message[offset + 1U]) << 16U)
                | (static_cast<std::uint32_t>(message[offset + 2U]) << 8U)
                | static_cast<std::uint32_t>(message[offset + 3U]);
        }
        for (std::size_t index = 16U; index < schedule.size(); ++index) {
            const std::uint32_t sigma0 = std::rotr(schedule[index - 15U], 7)
                ^ std::rotr(schedule[index - 15U], 18)
                ^ (schedule[index - 15U] >> 3U);
            const std::uint32_t sigma1 = std::rotr(schedule[index - 2U], 17)
                ^ std::rotr(schedule[index - 2U], 19)
                ^ (schedule[index - 2U] >> 10U);
            schedule[index] = schedule[index - 16U] + sigma0
                + schedule[index - 7U] + sigma1;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for (std::size_t index = 0; index < schedule.size(); ++index) {
            const std::uint32_t sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t temporary1 = h + sum1 + choose
                + sha256_round_constants[index] + schedule[index];
            const std::uint32_t sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temporary2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    constexpr std::string_view hexadecimal = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const std::uint32_t word : state) {
        for (std::size_t index = 0; index < 4U; ++index) {
            const unsigned int shift = static_cast<unsigned int>(24U - index * 8U);
            const std::uint8_t byte = static_cast<std::uint8_t>((word >> shift) & 0xFFU);
            result.push_back(hexadecimal[byte >> 4U]);
            result.push_back(hexadecimal[byte & 0x0FU]);
        }
    }
    return result;
}

/**
 * @brief Tests whether a canonical candidate stays beneath a canonical base.
 * @param base Canonical directory path.
 * @param candidate Canonical candidate path.
 * @return True if every base component prefixes the candidate.
 */
[[nodiscard]] bool is_descendant(
    const std::filesystem::path& base,
    const std::filesystem::path& candidate) {
    auto base_part = base.begin();
    auto candidate_part = candidate.begin();
    while (base_part != base.end()) {
        if (candidate_part == candidate.end() || *base_part != *candidate_part) {
            return false;
        }
        ++base_part;
        ++candidate_part;
    }
    return candidate_part != candidate.end();
}

/**
 * @brief Converts validated manifest UTF-8 bytes into a native filesystem path.
 * @param encoded_path UTF-8 path bytes retained by the staging contract.
 * @return Native path whose Windows conversion uses UTF-8 rather than the ANSI code page.
 * @throws std::bad_alloc if conversion storage cannot be allocated.
 */
[[nodiscard]] std::filesystem::path path_from_utf8(
    const std::string_view encoded_path) {
    std::u8string utf8_path;
    utf8_path.reserve(encoded_path.size());
    for (const char byte : encoded_path) {
        utf8_path.push_back(static_cast<char8_t>(static_cast<unsigned char>(byte)));
    }
    return std::filesystem::path{utf8_path};
}

/**
 * @brief Resolves a safe portable staged path below the manifest directory.
 * @param manifest_directory Canonical manifest parent directory.
 * @param encoded_path Forward-slash relative path from the manifest.
 * @param field_name Manifest path field used in diagnostics.
 * @return Canonical path to an existing regular file.
 */
[[nodiscard]] std::filesystem::path resolve_staged_path(
    const std::filesystem::path& manifest_directory,
    const std::string& encoded_path,
    const std::string_view field_name) {
    if (encoded_path.empty() || encoded_path.size() > maximum_staged_path_bytes
        || encoded_path.front() == '/' || encoded_path.back() == '/'
        || encoded_path.find("//") != std::string::npos
        || encoded_path.find('\\') != std::string::npos
        || encoded_path.find(':') != std::string::npos) {
        invalid_staged_input("Manifest key " + std::string(field_name)
            + " is not a safe portable relative path");
    }

    const std::filesystem::path relative = path_from_utf8(encoded_path);
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
        invalid_staged_input("Manifest key " + std::string(field_name)
            + " must be relative");
    }
    for (const std::filesystem::path& component : relative) {
        if (component == "." || component == ".." || component.empty()) {
            invalid_staged_input("Manifest key " + std::string(field_name)
                + " contains a forbidden path component");
        }
    }

    std::error_code error;
    const std::filesystem::path candidate = std::filesystem::canonical(
        manifest_directory / relative, error);
    if (error || !is_descendant(manifest_directory, candidate)) {
        invalid_staged_input("Manifest key " + std::string(field_name)
            + " escapes the staging directory or does not exist");
    }
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
        invalid_staged_input("Manifest key " + std::string(field_name)
            + " does not name a regular file");
    }
    return candidate;
}

/**
 * @brief Returns the next whitespace-delimited token from text.
 * @param text Complete text file.
 * @param cursor Updated scan position.
 * @return Next token, or an empty view at end of input.
 */
[[nodiscard]] std::string_view next_token(
    const std::string_view text,
    std::size_t& cursor) noexcept {
    while (cursor < text.size()) {
        const char byte = text[cursor];
        if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') {
            break;
        }
        ++cursor;
    }
    const std::size_t begin = cursor;
    while (cursor < text.size()) {
        const char byte = text[cursor];
        if (byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n') {
            break;
        }
        ++cursor;
    }
    return text.substr(begin, cursor - begin);
}

/**
 * @brief Parses exactly one float height per terrain sample.
 * @param text Exact staged height-file bytes.
 * @param sample_count Required sample count.
 * @return Row-major finite height values.
 */
[[nodiscard]] std::vector<float> parse_heights(
    const std::string_view text,
    const std::size_t sample_count) {
    std::vector<float> values;
    values.reserve(sample_count);
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < sample_count; ++index) {
        const std::string_view token = next_token(text, cursor);
        if (token.empty()) {
            invalid_staged_input("Height sample file has fewer values than the tile dimensions");
        }
        float value = 0.0F;
        const auto result = std::from_chars(
            token.data(), token.data() + token.size(), value, std::chars_format::general);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size()
            || !std::isfinite(value)) {
            invalid_staged_input("Height sample " + std::to_string(index)
                + " is not a finite float");
        }
        values.push_back(value);
    }
    if (!next_token(text, cursor).empty()) {
        invalid_staged_input("Height sample file has more values than the tile dimensions");
    }
    return values;
}

/**
 * @brief Parses exactly one canonical validity byte per terrain sample.
 * @param text Exact staged validity-file bytes.
 * @param sample_count Required sample count.
 * @return Row-major zero-or-one validity bytes.
 */
[[nodiscard]] std::vector<std::uint8_t> parse_validity(
    const std::string_view text,
    const std::size_t sample_count) {
    std::vector<std::uint8_t> values;
    values.reserve(sample_count);
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < sample_count; ++index) {
        const std::string_view token = next_token(text, cursor);
        if (token != "0" && token != "1") {
            invalid_staged_input("Validity sample " + std::to_string(index)
                + " must be exactly 0 or 1");
        }
        values.push_back(token == "1" ? 1U : 0U);
    }
    if (!next_token(text, cursor).empty()) {
        invalid_staged_input("Validity-mask file has more values than the tile dimensions");
    }
    return values;
}

/**
 * @brief Owns an atomic same-output compiler reservation directory.
 *
 * Creating a directory is an atomic no-replace operation on the filesystem.
 * Every cooperating compiler instance therefore has exactly one winner for a
 * given `output.gexworld.lock` path.
 */
class OutputReservation final {
public:
    /**
     * @brief Atomically acquires the output reservation.
     * @param output_path Final package path.
     * @throws CompilerError when another process owns or left the reservation.
     */
    explicit OutputReservation(const std::filesystem::path& output_path) {
        lock_path_ = output_path;
        lock_path_ += ".lock";
        std::error_code error;
        const bool created = std::filesystem::create_directory(lock_path_, error);
        if (error) {
            io_error("Cannot acquire output reservation " + lock_path_.string()
                + ": " + error.message());
        }
        if (!created) {
            io_error("Output reservation already exists: " + lock_path_.string());
        }
        owned_ = true;
    }

    /** Releases an owned reservation on exceptional exit without throwing. */
    ~OutputReservation() {
        std::error_code ignored;
        static_cast<void>(release(ignored));
    }

    OutputReservation(const OutputReservation&) = delete;
    OutputReservation& operator=(const OutputReservation&) = delete;
    OutputReservation(OutputReservation&&) = delete;
    OutputReservation& operator=(OutputReservation&&) = delete;

    /**
     * @brief Returns the private partial-file path inside the owned reservation.
     * @return `package.part` beneath the atomically created lock directory.
     * @throws std::bad_alloc if path construction cannot allocate memory.
     */
    [[nodiscard]] std::filesystem::path temporary_path() const {
        return lock_path_ / "package.part";
    }

    /**
     * @brief Removes the reservation directory explicitly.
     * @param error Receives a filesystem cleanup error.
     * @return True when no owned reservation remains.
     */
    [[nodiscard]] bool release(std::error_code& error) noexcept {
        error.clear();
        if (!owned_) {
            return true;
        }
        const bool removed = std::filesystem::remove(lock_path_, error);
        if (!error && (removed || !std::filesystem::exists(lock_path_, error))) {
            if (!error) {
                owned_ = false;
                return true;
            }
        }
        return false;
    }

private:
    /** Exact sibling lock-directory path. */
    std::filesystem::path lock_path_;

    /** Whether this object remains responsible for lock cleanup. */
    bool owned_{false};
};

/**
 * @brief Adds a filesystem cleanup failure to a primary diagnostic.
 * @param message Mutable primary diagnostic.
 * @param subject Cleanup action description.
 * @param error Cleanup error to append when set.
 */
void append_cleanup_error(
    std::string& message,
    const std::string_view subject,
    const std::error_code& error) {
    if (error) {
        message += "; additionally failed to ";
        message += subject;
        message += ": ";
        message += error.message();
    }
}

/**
 * @brief Removes a compiler-owned partial file and releases its reservation.
 * @param temporary Partial-file path, which may not exist.
 * @param reservation Owned output reservation.
 * @param message Primary failure diagnostic.
 */
[[noreturn]] void fail_publication(
    const std::filesystem::path& temporary,
    OutputReservation& reservation,
    std::string message) {
    std::error_code cleanup_error;
    static_cast<void>(std::filesystem::remove(temporary, cleanup_error));
    append_cleanup_error(message, "remove partial output", cleanup_error);
    cleanup_error.clear();
    static_cast<void>(reservation.release(cleanup_error));
    append_cleanup_error(message, "release output reservation", cleanup_error);
    io_error(message);
}

/**
 * @brief Writes and atomically publishes canonical bytes without replacement.
 * @param output_path Required new `.gexworld` path.
 * @param bytes Complete encoded package.
 */
void write_new_package(
    const std::filesystem::path& output_path,
    const std::vector<std::uint8_t>& bytes) {
    if (output_path.extension() != ".gexworld") {
        io_error("Output path must use the .gexworld extension");
    }

    std::error_code error;
    const std::filesystem::path parent = output_path.has_parent_path()
        ? output_path.parent_path()
        : std::filesystem::current_path(error);
    if (error || !std::filesystem::is_directory(parent, error) || error) {
        io_error("Output directory does not exist: " + parent.string());
    }

    OutputReservation reservation(output_path);
    if (std::filesystem::exists(output_path, error) || error) {
        std::string message = "Output path already exists or cannot be inspected: "
            + output_path.string();
        std::error_code release_error;
        static_cast<void>(reservation.release(release_error));
        append_cleanup_error(message, "release output reservation", release_error);
        io_error(message);
    }

    std::filesystem::path legacy_temporary = output_path;
    legacy_temporary += ".part";
    if (std::filesystem::exists(legacy_temporary, error) || error) {
        std::string message = "Partial output path already exists or cannot be inspected: "
            + legacy_temporary.string();
        std::error_code release_error;
        static_cast<void>(reservation.release(release_error));
        append_cleanup_error(message, "release output reservation", release_error);
        io_error(message);
    }

    const std::filesystem::path temporary = reservation.temporary_path();

    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            fail_publication(
                temporary, reservation, "Cannot create partial output: " + temporary.string());
        }
        if (!bytes.empty()) {
            stream.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        stream.close();
        if (!stream) {
            fail_publication(
                temporary,
                reservation,
                "Cannot completely write partial output: " + temporary.string());
        }
    }

    error.clear();
    std::filesystem::create_hard_link(temporary, output_path, error);
    if (error) {
        fail_publication(
            temporary,
            reservation,
            "Cannot atomically publish new output package " + output_path.string()
                + ": " + error.message());
    }

    error.clear();
    static_cast<void>(std::filesystem::remove(temporary, error));
    if (error) {
        std::string message = "Package was published but partial-link cleanup failed: "
            + temporary.string() + ": " + error.message();
        std::error_code release_error;
        static_cast<void>(reservation.release(release_error));
        append_cleanup_error(message, "release output reservation", release_error);
        io_error(message);
    }

    error.clear();
    if (!reservation.release(error)) {
        io_error("Package was published but output reservation cleanup failed: "
            + error.message());
    }
}

} // namespace

CompilerError::CompilerError(const CompilerErrorCode code, std::string message)
    : std::runtime_error(std::move(message)),
      code_(code) {}

CompilerErrorCode CompilerError::code() const noexcept {
    return code_;
}

world_format::WorldPackage load_staging_world(
    const std::filesystem::path& manifest_path) {
    const std::string manifest_text = read_bounded_text_file(
        manifest_path, maximum_manifest_bytes, "staging manifest", false);
    if (!is_valid_utf8(manifest_text)) {
        invalid_manifest("Staging manifest is not valid UTF-8");
    }
    for (std::size_t index = 0; index < manifest_text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(manifest_text[index]);
        if ((byte < 0x20U && byte != static_cast<unsigned char>('\n')
                && byte != static_cast<unsigned char>('\r'))
            || byte == 0x7FU) {
            invalid_manifest("Staging manifest contains a forbidden control byte");
        }
        if (byte == static_cast<unsigned char>('\r')
            && (index + 1U >= manifest_text.size() || manifest_text[index + 1U] != '\n')) {
            invalid_manifest("Staging manifest contains a carriage return outside CRLF");
        }
    }
    const ManifestFields fields = parse_manifest(manifest_text);

    require_exact(fields, "format", "game_ex_staging_manifest");
    require_exact(fields, "version", "1");
    require_exact(fields, "crs", "EPSG:5514");
    require_exact(fields, "terrain.layer_count", "1");
    require_exact(fields, "terrain.tile_count", "1");
    require_exact(fields, "stage.axis_order", "X,Y");
    require_exact(fields, "stage.horizontal_units", "metre");
    require_exact(fields, "stage.vertical_units", "metre");
    require_exact(fields, "stage.no_data", "validity_mask");

    world_format::WorldPackage package;
    world_format::TerrainTile& terrain = package.terrain;
    terrain.layer_id = field(fields, "terrain.layer_id");
    terrain.tile_id = field(fields, "terrain.tile_id");
    validate_text_field(terrain.layer_id, "terrain.layer_id", true);
    validate_text_field(terrain.tile_id, "terrain.tile_id", true);

    terrain.columns = parse_u32(
        field(fields, "terrain.tile.columns"), "terrain.tile.columns");
    terrain.rows = parse_u32(field(fields, "terrain.tile.rows"), "terrain.tile.rows");
    if (terrain.columns == 0 || terrain.rows == 0
        || terrain.columns > world_format::maximum_tile_dimension
        || terrain.rows > world_format::maximum_tile_dimension) {
        invalid_manifest("Terrain dimensions must be between 1 and the documented limit");
    }
    const std::uint64_t sample_count = static_cast<std::uint64_t>(terrain.columns)
        * static_cast<std::uint64_t>(terrain.rows);
    if (sample_count > world_format::maximum_tile_samples
        || sample_count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        invalid_manifest("Terrain sample count exceeds the documented limit");
    }

    terrain.origin_x = parse_double(
        field(fields, "terrain.tile.origin_x"), "terrain.tile.origin_x");
    terrain.origin_y = parse_double(
        field(fields, "terrain.tile.origin_y"), "terrain.tile.origin_y");
    terrain.origin_z = parse_double(
        field(fields, "terrain.tile.origin_z"), "terrain.tile.origin_z");
    terrain.spacing_x = parse_double(
        field(fields, "terrain.tile.spacing_x"), "terrain.tile.spacing_x");
    terrain.spacing_y = parse_double(
        field(fields, "terrain.tile.spacing_y"), "terrain.tile.spacing_y");
    if (terrain.spacing_x <= 0.0 || terrain.spacing_y <= 0.0) {
        invalid_manifest("Terrain spacing values must be greater than zero");
    }
    const double maximum_x = terrain.origin_x
        + terrain.spacing_x * static_cast<double>(terrain.columns - 1U);
    const double maximum_y = terrain.origin_y
        + terrain.spacing_y * static_cast<double>(terrain.rows - 1U);
    if (!std::isfinite(maximum_x) || !std::isfinite(maximum_y)) {
        invalid_manifest("Derived terrain bounds must be finite");
    }

    world_format::Provenance& provenance = package.provenance;
    provenance.provider = field(fields, "provenance.provider");
    provenance.product = field(fields, "provenance.product");
    provenance.edition = field(fields, "provenance.edition");
    provenance.source_uri = field(fields, "provenance.source_uri");
    provenance.acquisition_timestamp = field(fields, "provenance.acquired_utc");
    provenance.licence_name = field(fields, "provenance.licence_name");
    provenance.licence_uri = field(fields, "provenance.licence_uri");
    provenance.attribution = field(fields, "provenance.attribution");
    provenance.rights_statement = field(fields, "provenance.rights");
    provenance.source_filename = field(fields, "provenance.source_filename");
    provenance.source_byte_size = parse_u64(
        field(fields, "provenance.source_byte_size"), "provenance.source_byte_size");
    provenance.source_sha256 = field(fields, "provenance.source_sha256");
    provenance.source_classification = field(fields, "provenance.source_classification");
    provenance.stage_classification = field(fields, "provenance.stage_classification");
    provenance.source_crs = field(fields, "source.crs");
    provenance.source_axis_order = field(fields, "source.axis_order");
    provenance.source_horizontal_units = field(fields, "source.horizontal_units");
    provenance.source_vertical_reference = field(fields, "source.vertical_reference");
    provenance.source_vertical_units = field(fields, "source.vertical_units");
    provenance.source_no_data_convention = field(fields, "source.no_data");
    provenance.stage_axis_order = field(fields, "stage.axis_order");
    provenance.stage_horizontal_units = field(fields, "stage.horizontal_units");
    provenance.stage_vertical_reference = field(fields, "stage.vertical_reference");
    provenance.stage_vertical_units = field(fields, "stage.vertical_units");
    provenance.stage_no_data_convention = field(fields, "stage.no_data");
    provenance.resolution = field(fields, "terrain.resolution");
    provenance.bounds = field(fields, "spatial.bounds");
    provenance.epoch = field(fields, "spatial.epoch");
    provenance.transformation = field(fields, "provenance.transformation");
    provenance.responsible_owner = field(fields, "provenance.responsible_owner");
    provenance.staged_heights_path = field(fields, "terrain.tile.heights");
    provenance.staged_heights_byte_size = parse_u64(
        field(fields, "staged.heights.byte_size"), "staged.heights.byte_size");
    provenance.staged_heights_sha256 = field(fields, "staged.heights.sha256");
    provenance.staged_validity_path = field(fields, "terrain.tile.validity");
    provenance.staged_validity_byte_size = parse_u64(
        field(fields, "staged.validity.byte_size"), "staged.validity.byte_size");
    provenance.staged_validity_sha256 = field(fields, "staged.validity.sha256");
    validate_text_field(provenance.provider, "provenance.provider", false);
    validate_text_field(provenance.product, "provenance.product", false);
    validate_text_field(provenance.edition, "provenance.edition", false);
    validate_text_field(provenance.source_uri, "provenance.source_uri", false);
    validate_text_field(provenance.acquisition_timestamp, "provenance.acquired_utc", false);
    validate_text_field(provenance.licence_name, "provenance.licence_name", false);
    validate_text_field(provenance.licence_uri, "provenance.licence_uri", false);
    validate_text_field(provenance.attribution, "provenance.attribution", false);
    validate_text_field(provenance.rights_statement, "provenance.rights", false);
    validate_text_field(provenance.source_filename, "provenance.source_filename", false);
    validate_text_field(provenance.source_sha256, "provenance.source_sha256", false);
    validate_text_field(
        provenance.source_classification, "provenance.source_classification", false);
    validate_text_field(
        provenance.stage_classification, "provenance.stage_classification", false);
    validate_text_field(provenance.source_crs, "source.crs", false);
    validate_text_field(provenance.source_axis_order, "source.axis_order", false);
    validate_text_field(provenance.source_horizontal_units, "source.horizontal_units", false);
    validate_text_field(provenance.source_vertical_reference, "source.vertical_reference", false);
    validate_text_field(provenance.source_vertical_units, "source.vertical_units", false);
    validate_text_field(provenance.source_no_data_convention, "source.no_data", false);
    validate_text_field(provenance.stage_axis_order, "stage.axis_order", false);
    validate_text_field(provenance.stage_horizontal_units, "stage.horizontal_units", false);
    validate_text_field(provenance.stage_vertical_reference, "stage.vertical_reference", false);
    validate_text_field(provenance.stage_vertical_units, "stage.vertical_units", false);
    validate_text_field(provenance.stage_no_data_convention, "stage.no_data", false);
    validate_text_field(provenance.resolution, "terrain.resolution", false);
    validate_text_field(provenance.bounds, "spatial.bounds", false);
    validate_text_field(provenance.epoch, "spatial.epoch", false);
    validate_text_field(provenance.transformation, "provenance.transformation", false);
    validate_text_field(provenance.responsible_owner, "provenance.responsible_owner", false);
    validate_text_field(provenance.staged_heights_path, "terrain.tile.heights", false);
    validate_text_field(provenance.staged_heights_sha256, "staged.heights.sha256", false);
    validate_text_field(provenance.staged_validity_path, "terrain.tile.validity", false);
    validate_text_field(provenance.staged_validity_sha256, "staged.validity.sha256", false);
    validate_acquired_utc(provenance.acquisition_timestamp);
    validate_source_filename(provenance.source_filename);
    validate_sha256(provenance.source_sha256, "provenance.source_sha256");
    validate_sha256(provenance.staged_heights_sha256, "staged.heights.sha256");
    validate_sha256(provenance.staged_validity_sha256, "staged.validity.sha256");

    std::error_code error;
    const std::filesystem::path manifest_directory = std::filesystem::canonical(
        manifest_path.parent_path().empty()
            ? std::filesystem::current_path(error)
            : manifest_path.parent_path(),
        error);
    if (error) {
        io_error("Cannot resolve the staging-manifest directory");
    }
    const std::filesystem::path heights_path = resolve_staged_path(
        manifest_directory,
        field(fields, "terrain.tile.heights"),
        "terrain.tile.heights");
    const std::filesystem::path validity_path = resolve_staged_path(
        manifest_directory,
        field(fields, "terrain.tile.validity"),
        "terrain.tile.validity");

    const auto count = static_cast<std::size_t>(sample_count);
    const std::string heights_text = read_bounded_text_file(
        heights_path, maximum_staged_text_bytes, "height sample file", true);
    const std::string validity_text = read_bounded_text_file(
        validity_path, maximum_staged_text_bytes, "validity-mask file", true);
    if (heights_text.size() != provenance.staged_heights_byte_size
        || sha256_hex(heights_text) != provenance.staged_heights_sha256) {
        invalid_staged_input("Height sample file does not match its declared byte size and SHA-256");
    }
    if (validity_text.size() != provenance.staged_validity_byte_size
        || sha256_hex(validity_text) != provenance.staged_validity_sha256) {
        invalid_staged_input("Validity-mask file does not match its declared byte size and SHA-256");
    }
    terrain.heights = parse_heights(heights_text, count);
    terrain.validity = parse_validity(validity_text, count);
    return package;
}

void compile_world(
    const std::filesystem::path& manifest_path,
    const std::filesystem::path& output_path) {
    const world_format::WorldPackage package = load_staging_world(manifest_path);
    try {
        const std::vector<std::uint8_t> bytes =
            world_format::detail::encode_world_package(package);
        write_new_package(output_path, bytes);
    } catch (const world_format::PackageError& error) {
        throw CompilerError(
            CompilerErrorCode::invalid_manifest,
            std::string("Validated staging data cannot be encoded: ") + error.what());
    }
}

} // namespace game_ex::world_compiler
