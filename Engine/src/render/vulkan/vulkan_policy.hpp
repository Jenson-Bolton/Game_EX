/**
 * @file vulkan_policy.hpp
 * @brief Deterministic private selection policies for the Vulkan backend.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace game_ex::render::vulkan_detail {

/**
 * @brief Backend-neutral facts about one Vulkan queue family.
 */
struct QueueFamilySupport final {
    /** Vulkan queue-family index. */
    std::uint32_t index{};

    /** Whether the family accepts graphics operations. */
    bool graphics{};

    /** Whether the family can present to the selected surface. */
    bool presentation{};
};

/**
 * @brief Deterministic graphics and presentation queue selection.
 */
struct QueueSelection final {
    /** Selected graphics queue-family index. */
    std::uint32_t graphics_family{};

    /** Selected presentation queue-family index. */
    std::uint32_t presentation_family{};
};

/**
 * @brief Chooses queue families, preferring one combined family.
 * @param families Queue capabilities in arbitrary enumeration order.
 * @return Lowest combined family, otherwise the lowest separate pair, or empty.
 */
[[nodiscard]] std::optional<QueueSelection>
choose_queue_families(std::span<const QueueFamilySupport> families) noexcept;

/**
 * @brief Stable suitability facts for one enumerated physical device.
 */
struct PhysicalDeviceCandidate final {
    /** Caller-owned index used to recover the native device. */
    std::size_t source_index{};

    /** Vulkan device class used for deterministic preference. */
    VkPhysicalDeviceType type{VK_PHYSICAL_DEVICE_TYPE_OTHER};

    /** Stable Vulkan device UUID, or all zeroes when unavailable. */
    std::array<std::uint8_t, VK_UUID_SIZE> uuid{};

    /** PCI or implementation vendor identifier. */
    std::uint32_t vendor_id{};

    /** Vendor-defined physical device identifier. */
    std::uint32_t device_id{};

    /** Stable diagnostic device name used only as a final tie-break. */
    std::string name;

    /** Whether all baseline, queue, surface, and usage requirements passed. */
    bool suitable{};
};

/**
 * @brief Chooses one suitable device independently of enumeration order.
 * @param candidates Evaluated physical-device facts.
 * @return Source index of the preferred candidate, or empty when none are
 * suitable.
 */
[[nodiscard]] std::optional<std::size_t>
choose_physical_device(std::span<const PhysicalDeviceCandidate> candidates) noexcept;

/**
 * @brief Chooses an sRGB surface format through a stable preference order.
 * @param formats Non-empty set of formats supported by the surface.
 * @return Preferred supported sRGB format.
 * @throws std::invalid_argument if no sRGB/nonlinear combination is supported.
 */
[[nodiscard]] VkSurfaceFormatKHR choose_surface_format(std::span<const VkSurfaceFormatKHR> formats);

/**
 * @brief Resolves a swapchain pixel extent from capabilities and drawable size.
 * @param capabilities Current surface capabilities.
 * @param drawable_extent Current SDL drawable extent in pixels.
 * @return Fixed surface extent or a clamped variable extent.
 */
[[nodiscard]] VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                                                 VkExtent2D drawable_extent) noexcept;

/**
 * @brief Chooses one more image than the surface minimum when permitted.
 * @param capabilities Current surface capabilities.
 * @return Valid deterministic swapchain image request.
 * @throws std::invalid_argument for inconsistent non-zero image limits.
 */
[[nodiscard]] std::uint32_t
choose_swapchain_image_count(const VkSurfaceCapabilitiesKHR& capabilities);

/**
 * @brief Chooses a supported composite-alpha mode through a stable preference.
 * @param supported Supported composite-alpha flags from surface capabilities.
 * @return Preferred supported mode.
 * @throws std::invalid_argument when no known mode is supported.
 */
[[nodiscard]] VkCompositeAlphaFlagBitsKHR
choose_composite_alpha(VkCompositeAlphaFlagsKHR supported);

} // namespace game_ex::render::vulkan_detail
