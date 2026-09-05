/**
 * @file hex_fixture.hpp
 * @brief Dependency-free textual hexadecimal fixture loader for WorldFormat tests.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace game_ex::world_test_detail {

/**
 * @brief Converts one ASCII hexadecimal digit to its numeric value.
 * @param digit Character to decode.
 * @return Value from zero through fifteen.
 * @throws std::runtime_error if `digit` is not hexadecimal ASCII.
 */
[[nodiscard]] inline std::uint8_t hex_nibble(const char digit) {
    if (digit >= '0' && digit <= '9') {
        return static_cast<std::uint8_t>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<std::uint8_t>(digit - 'a' + 10);
    }
    if (digit >= 'A' && digit <= 'F') {
        return static_cast<std::uint8_t>(digit - 'A' + 10);
    }
    throw std::runtime_error("Golden fixture contains a non-hexadecimal byte");
}

/**
 * @brief Loads bytes from a line-oriented textual hexadecimal fixture.
 *
 * Empty lines and lines whose first non-whitespace byte is `#` are ignored.
 * Spaces, horizontal tabs, and carriage returns may separate hexadecimal digits.
 *
 * @param path Textual fixture path.
 * @return Exact decoded byte sequence.
 * @throws std::runtime_error if the file cannot be read or contains malformed hex.
 * @throws std::bad_alloc if result storage cannot be allocated.
 */
[[nodiscard]] inline std::vector<std::uint8_t> read_hex_fixture(
    const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot open golden fixture: " + path.string());
    }

    std::vector<std::uint8_t> bytes;
    std::string line;
    bool has_high_nibble = false;
    std::uint8_t high_nibble = 0U;
    while (std::getline(stream, line)) {
        const std::size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        for (std::size_t index = first; index < line.size(); ++index) {
            const char digit = line[index];
            if (digit == ' ' || digit == '\t' || digit == '\r') {
                continue;
            }
            const std::uint8_t nibble = hex_nibble(digit);
            if (!has_high_nibble) {
                high_nibble = nibble;
                has_high_nibble = true;
            } else {
                bytes.push_back(static_cast<std::uint8_t>((high_nibble << 4U) | nibble));
                has_high_nibble = false;
            }
        }
    }
    if (stream.bad()) {
        throw std::runtime_error("Cannot completely read golden fixture: " + path.string());
    }
    if (has_high_nibble || bytes.empty()) {
        throw std::runtime_error("Golden fixture has empty or odd-length hexadecimal data");
    }
    return bytes;
}

} // namespace game_ex::world_test_detail
