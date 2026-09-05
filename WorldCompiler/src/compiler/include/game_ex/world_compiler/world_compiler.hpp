/**
 * @file world_compiler.hpp
 * @brief Offline staging-manifest validation and deterministic package compilation.
 */

#pragma once

#include "game_ex/world_format/world_package.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @defgroup world_compiler World compiler
 * @brief Offline validation and portable package construction.
 */

namespace game_ex::world_compiler {

/**
 * @brief Stable error categories for manifest and staged-input processing.
 * @ingroup world_compiler
 */
enum class CompilerErrorCode {
    /** A required file could not be opened, read, or written. */
    io,

    /** Manifest structure or metadata violates the staging contract. */
    invalid_manifest,

    /** A referenced terrain or validity file is unsafe or invalid. */
    invalid_staged_input
};

/**
 * @brief Classified offline compilation error.
 * @ingroup world_compiler
 */
class CompilerError final : public std::runtime_error {
public:
    /**
     * @brief Creates a classified compiler error.
     * @param code Stable error category.
     * @param message Human-readable diagnostic.
     */
    CompilerError(CompilerErrorCode code, std::string message);

    /**
     * @brief Returns the stable compiler category.
     * @return Error category supplied at construction.
     */
    [[nodiscard]] CompilerErrorCode code() const noexcept;

private:
    /** Stable compiler category. */
    CompilerErrorCode code_;
};

/**
 * @brief Strictly parses a manifest and all referenced staged inputs.
 * @param manifest_path Path to the staging manifest.
 * @return Validated logical world package ready for deterministic encoding.
 * @throws CompilerError for manifest, staged-input, or I/O failures.
 * @throws std::bad_alloc when bounded allocations cannot be satisfied.
 * @ingroup world_compiler
 */
[[nodiscard]] world_format::WorldPackage load_staging_world(
    const std::filesystem::path& manifest_path);

/**
 * @brief Validates, deterministically encodes, and writes one world package.
 * @param manifest_path Path to the strict staging manifest.
 * @param output_path Destination `.gexworld` path.
 * @throws CompilerError for input validation or output I/O failure.
 * @throws std::bad_alloc when bounded allocations cannot be satisfied.
 * @ingroup world_compiler
 */
void compile_world(
    const std::filesystem::path& manifest_path,
    const std::filesystem::path& output_path);

} // namespace game_ex::world_compiler
