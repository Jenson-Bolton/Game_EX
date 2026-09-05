/**
 * @file package_layout.hpp
 * @brief Internal canonical `.gexworld` wire-layout constants and CRC primitive.
 *
 * Version 0.2 begins with this 128-byte header. Every numeric value is explicit
 * little-endian; no native structure is ever dumped:
 *
 * | Offset | Bytes | Value |
 * |------:|------:|:------|
 * | 0 | 8 | `GAMEEXWD` magic |
 * | 8 | 2 | major version |
 * | 10 | 2 | minor version |
 * | 12 | 4 | header size (128) |
 * | 16 | 8 | complete file size |
 * | 24 | 4 | CRC-32/ISO-HDLC; treated as zero while hashing the whole file |
 * | 28 | 4 | reserved flags (zero) |
 * | 32 | 4 | horizontal EPSG code (5514) |
 * | 36 | 4 | tile columns |
 * | 40 | 4 | tile rows |
 * | 44 | 4 | metadata string count (35) |
 * | 48 | 8 | EPSG:5514 origin X, IEEE-754 binary64 |
 * | 56 | 8 | EPSG:5514 origin Y, IEEE-754 binary64 |
 * | 64 | 8 | X sample spacing, IEEE-754 binary64 |
 * | 72 | 8 | Y sample spacing, IEEE-754 binary64 |
 * | 80 | 8 | local-height payload offset |
 * | 88 | 8 | validity payload offset |
 * | 96 | 8 | metadata block offset |
 * | 104 | 8 | metadata block byte size |
 * | 112 | 8 | sample count |
 * | 120 | 8 | absolute vertical-datum origin Z, IEEE-754 binary64 |
 *
 * The payload is row-major binary32 local-height offsets followed by one zero
 * or one validity byte per sample. Metadata begins with three little-endian u64
 * byte sizes (original source, staged heights, staged validity). It then stores
 * 35 UTF-8 strings, each as a little-endian u32 byte length followed by bytes,
 * in this exact order: layer ID, tile ID, provider, product, edition, source URI,
 * acquisition UTC, licence name, licence URI, attribution, rights, original
 * filename, original SHA-256, source classification, stage classification,
 * source CRS, source axis order, source horizontal units, source vertical
 * reference, source vertical units, source no-data, stage axis order, stage
 * horizontal units, stage vertical reference, stage vertical units, stage
 * no-data, resolution, bounds, epoch, transformation, responsible owner,
 * staged-height path, staged-height SHA-256, staged-validity path, and
 * staged-validity SHA-256.
 */

#pragma once

#include "game_ex/world_format/world_package.hpp"

#include <cstdint>
#include <span>

namespace game_ex::world_format::detail {

/** Fixed byte length of the version 0.2 package header. */
inline constexpr std::uint32_t encoded_header_size = 128;

/** Byte offset of the CRC-32 field within the fixed header. */
inline constexpr std::size_t checksum_offset = 24;

/** Number of fixed-order strings in the metadata block. */
inline constexpr std::uint32_t metadata_string_count = 35;

/**
 * @brief Computes standard CRC-32/ISO-HDLC over an arbitrary byte sequence.
 * @param bytes Exact bytes to hash without package-field substitution.
 * @return CRC value using polynomial `0x04C11DB7`, reflected input/output,
 * initial value `0xffffffff`, and final XOR `0xffffffff`.
 */
[[nodiscard]] std::uint32_t crc32_iso_hdlc(
    std::span<const std::uint8_t> bytes) noexcept;

} // namespace game_ex::world_format::detail
