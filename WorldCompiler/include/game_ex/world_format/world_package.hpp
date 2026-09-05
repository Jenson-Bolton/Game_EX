/**
 * @file world_package.hpp
 * @brief Portable runtime reader and logical model for `.gexworld` packages.
 */

#pragma once

#include "game_ex/world_format/world_header.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace game_ex::world_format {

/** EPSG code required by the first terrain staging package. */
inline constexpr std::uint32_t staging_crs_epsg = 5514;

/** Maximum columns or rows accepted for one staging terrain tile. */
inline constexpr std::uint32_t maximum_tile_dimension = 4096;

/** Maximum samples accepted for one staging terrain tile. */
inline constexpr std::uint64_t maximum_tile_samples = 4'194'304;

/** Maximum encoded package size accepted by the runtime reader. */
inline constexpr std::uint64_t maximum_package_bytes = 64ULL * 1024ULL * 1024ULL;

/** Maximum bytes accepted for any embedded identifier or provenance string. */
inline constexpr std::size_t maximum_metadata_string_bytes = 4096;

/**
 * @brief Stable package-reader error classification.
 * @ingroup world_format
 */
enum class PackageErrorCode {
    /** The package could not be opened or completely read. */
    io,

    /** The byte sequence violates the documented package format. */
    invalid_format,

    /** The package version is newer or otherwise unsupported. */
    unsupported_version,

    /** The stored integrity checksum does not match the package bytes. */
    integrity
};

/**
 * @brief Error raised while opening or decoding a runtime world package.
 * @ingroup world_format
 */
class PackageError final : public std::runtime_error {
public:
    /**
     * @brief Creates a classified package-reader error.
     * @param code Stable error category.
     * @param message Human-readable diagnostic.
     */
    PackageError(PackageErrorCode code, std::string message);

    /**
     * @brief Returns the stable package-reader category.
     * @return Error category supplied at construction.
     */
    [[nodiscard]] PackageErrorCode code() const noexcept;

private:
    /** Stable package-reader category. */
    PackageErrorCode code_;
};

/**
 * @brief Source identity and reuse information embedded in a package.
 * @ingroup world_format
 */
struct Provenance final {
    /** Organisation or authority that supplied the source. */
    std::string provider;

    /** Provider-defined dataset or product name. */
    std::string product;

    /** Provider-defined edition, release, or snapshot identifier. */
    std::string edition;

    /** Source locator or stable project-local URI. */
    std::string source_uri;

    /** UTC acquisition timestamp written as `YYYY-MM-DDTHH:MM:SSZ`. */
    std::string acquisition_timestamp;

    /** License identifier or concise license name. */
    std::string licence_name;

    /** Stable locator for the applicable license text. */
    std::string licence_uri;

    /** Attribution text that consumers must retain. */
    std::string attribution;

    /** Redistribution conditions or other applicable rights statement. */
    std::string rights_statement;

    /** Restricted path-free name recorded for the original acquired source. */
    std::string source_filename;

    /** Exact byte size of the original acquired source file. */
    std::uint64_t source_byte_size{0};

    /** Lowercase hexadecimal SHA-256 of the original acquired source file. */
    std::string source_sha256;

    /** Classification assigned to the acquired source. */
    std::string source_classification;

    /** Classification assigned to this normalised staging data. */
    std::string stage_classification;

    /** Original source coordinate-reference-system identifier. */
    std::string source_crs;

    /** Original source horizontal coordinate-axis order. */
    std::string source_axis_order;

    /** Original source horizontal coordinate units. */
    std::string source_horizontal_units;

    /** Original source vertical reference or explicit not-applicable value. */
    std::string source_vertical_reference;

    /** Original source vertical units or explicit not-applicable value. */
    std::string source_vertical_units;

    /** Original source no-data convention. */
    std::string source_no_data_convention;

    /** Normalised stage coordinate-axis order for EPSG:5514. */
    std::string stage_axis_order;

    /** Normalised stage horizontal coordinate units. */
    std::string stage_horizontal_units;

    /** Normalised stage absolute elevation datum used by `origin_z`. */
    std::string stage_vertical_reference;

    /** Normalised stage elevation and local-offset units. */
    std::string stage_vertical_units;

    /** Normalised stage no-data convention represented by the validity mask. */
    std::string stage_no_data_convention;

    /** Human-readable source/stage resolution statement. */
    std::string resolution;

    /**
     * Declared source/stage bounds statement retained as provenance.
     *
     * This string is not trusted for runtime geometry. Runtime X/Y bounds are
     * derived from the numeric tile origin, spacing, and dimensions.
     */
    std::string bounds;

    /** Dataset epoch, or a documented not-applicable value. */
    std::string epoch;

    /** Ordered transformation steps including tool names and versions. */
    std::string transformation;

    /** Project role or person responsible for the staged derivative. */
    std::string responsible_owner;

    /** Portable manifest-relative path of the staged height text. */
    std::string staged_heights_path;

    /** Exact verified byte size of the staged height text. */
    std::uint64_t staged_heights_byte_size{0};

    /** Verified lowercase hexadecimal SHA-256 of the staged height text. */
    std::string staged_heights_sha256;

    /** Portable manifest-relative path of the staged validity text. */
    std::string staged_validity_path;

    /** Exact verified byte size of the staged validity text. */
    std::uint64_t staged_validity_byte_size{0};

    /** Verified lowercase hexadecimal SHA-256 of the staged validity text. */
    std::string staged_validity_sha256;
};

/**
 * @brief One row-major terrain tile decoded for runtime use.
 *
 * Horizontal world coordinates use double-precision EPSG:5514 metadata. Samples
 * are row-major with `index = row * columns + column`; columns advance in +X and
 * rows advance in +Y. Runtime X/Y bounds are therefore `origin_x`/`origin_y` to
 * `origin + positive spacing * (dimension - 1)`. Height samples are tile-local
 * float offsets; validity contains one canonical zero or one byte per height.
 *
 * @ingroup world_format
 */
struct TerrainTile final {
    /** Stable terrain-layer identifier. */
    std::string layer_id;

    /** Stable tile identifier within the terrain layer. */
    std::string tile_id;

    /** Number of samples in each row. */
    std::uint32_t columns{0};

    /** Number of sample rows. */
    std::uint32_t rows{0};

    /** EPSG:5514 X coordinate of the first sample. */
    double origin_x{0.0};

    /** EPSG:5514 Y coordinate of the first sample. */
    double origin_y{0.0};

    /** Absolute vertical-datum elevation from which local heights are offset. */
    double origin_z{0.0};

    /** Positive X distance in metres between adjacent columns. */
    double spacing_x{0.0};

    /** Positive Y distance in metres between adjacent rows. */
    double spacing_y{0.0};

    /**
     * Row-major IEEE-754 binary32 offsets from `origin_z`.
     *
     * A valid sample's absolute elevation is `origin_z + heights[index]`,
     * evaluated in double precision by consumers that need world coordinates.
     */
    std::vector<float> heights;

    /** Row-major validity bytes constrained to zero or one. */
    std::vector<std::uint8_t> validity;
};

/**
 * @brief Logical contents of the first portable world-package slice.
 * @ingroup world_format
 */
struct WorldPackage final {
    /** Decoded identity and compatibility version. */
    WorldHeader header;

    /** Coordinate reference system EPSG code; currently exactly 5514. */
    std::uint32_t crs_epsg{staging_crs_epsg};

    /** The format's single terrain layer and single tile. */
    TerrainTile terrain;

    /** Embedded source and licensing record. */
    Provenance provenance;

    /** Stored CRC-32 integrity value. */
    std::uint32_t checksum{0};
};

/**
 * @brief Opens, verifies, and decodes a portable runtime package.
 * @param path Package file to read.
 * @return Fully validated logical package.
 * @throws PackageError for I/O, format, version, or integrity failure.
 * @throws std::bad_alloc if bounded package allocation cannot be satisfied.
 * @ingroup world_format
 */
[[nodiscard]] WorldPackage read_world_package(const std::filesystem::path& path);

} // namespace game_ex::world_format
