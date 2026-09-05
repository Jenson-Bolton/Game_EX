/**
 * @file vulkan_renderer.hpp
 * @brief Public composition entry point for the SDL/Vulkan 1.3 renderer backend.
 */

#pragma once

#include "game_ex/render/renderer_factory.hpp"

#include <memory>

namespace game_ex::render {

/**
 * @brief Controls use of the optional Khronos Vulkan validation layer.
 * @ingroup render_vulkan
 */
enum class VulkanValidationMode {
    /** Do not request API validation. */
    disabled,

    /** Enable validation when both the layer and debug-utils extension exist. */
    optional,

    /** Fail startup unless validation and debug-utils are available. */
    required
};

/**
 * @brief Vulkan-specific startup policy that contains no native Vulkan types.
 * @ingroup render_vulkan
 */
struct VulkanRendererConfiguration final {
    /** Validation-layer availability policy. */
    VulkanValidationMode validation{VulkanValidationMode::disabled};
};

/**
 * @brief Creates the Vulkan 1.3 renderer factory.
 * @param configuration Validation policy copied into each created renderer.
 * @return Uniquely owned factory requiring a Vulkan-capable SDL window.
 * @throws std::bad_alloc if factory allocation fails.
 * @ingroup render_vulkan
 */
[[nodiscard]] std::unique_ptr<RendererFactory>
create_vulkan_renderer_factory(VulkanRendererConfiguration configuration = {});

} // namespace game_ex::render
