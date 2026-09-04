/**
 * @file sdl_platform.hpp
 * @brief Factory for the SDL3 platform backend.
 */

#pragma once

#include "game_ex/platform/platform.hpp"

#include <memory>
#include <string>

namespace game_ex::platform {

/**
 * @brief Metadata supplied to SDL before platform initialisation.
 * @ingroup platform_sdl
 */
struct SdlPlatformSpecification final {
    /** Human-readable application name used by the host platform. */
    std::string application_name;

    /** Semantic application version displayed to the host platform. */
    std::string application_version;

    /** Reverse-DNS application identifier. */
    std::string application_identifier;
};

/**
 * @brief Creates and initialises the SDL3 implementation of Platform.
 * @param specification Application metadata reported through SDL3.
 * @return A uniquely owned initialised platform runtime.
 * @throws std::invalid_argument if required metadata is empty.
 * @throws std::runtime_error if SDL3 cannot initialise its video subsystem.
 *
 * This function must be called on the process main thread. The returned runtime
 * and all windows created by it must remain on that thread.
 *
 * @ingroup platform_sdl
 */
[[nodiscard]] std::unique_ptr<Platform> create_sdl_platform(
    const SdlPlatformSpecification& specification);

} // namespace game_ex::platform
