/**
 * @file vulkan_policy.cpp
 * @brief Deterministic private Vulkan policy implementation.
 */

#include "vulkan_policy.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace game_ex::render::vulkan_detail {
namespace {

/**
 * @brief Maps Vulkan device classes to stable preference ranks.
 * @param type Vulkan physical-device class.
 * @return Lower value for a more desirable class.
 */
[[nodiscard]] int device_type_rank(const VkPhysicalDeviceType type) noexcept {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 0;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 1;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return 3;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:
        return 4;
    }
}

} // namespace

std::optional<QueueSelection>
choose_queue_families(const std::span<const QueueFamilySupport> families) noexcept {
    std::optional<std::uint32_t> combined;
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> presentation;

    for (const QueueFamilySupport& family : families) {
        if (family.graphics && family.presentation
            && (!combined.has_value() || family.index < combined.value())) {
            combined = family.index;
        }
        if (family.graphics && (!graphics.has_value() || family.index < graphics.value())) {
            graphics = family.index;
        }
        if (family.presentation
            && (!presentation.has_value() || family.index < presentation.value())) {
            presentation = family.index;
        }
    }

    if (combined.has_value()) {
        return QueueSelection{combined.value(), combined.value()};
    }
    if (graphics.has_value() && presentation.has_value()) {
        return QueueSelection{graphics.value(), presentation.value()};
    }
    return std::nullopt;
}

std::optional<std::size_t>
choose_physical_device(const std::span<const PhysicalDeviceCandidate> candidates) noexcept {
    const PhysicalDeviceCandidate* selected = nullptr;
    for (const PhysicalDeviceCandidate& candidate : candidates) {
        if (!candidate.suitable) {
            continue;
        }

        if (selected == nullptr
            || std::tuple{device_type_rank(candidate.type), candidate.uuid, candidate.vendor_id,
                          candidate.device_id, candidate.name, candidate.source_index}
                   < std::tuple{device_type_rank(selected->type), selected->uuid,
                                selected->vendor_id, selected->device_id, selected->name,
                                selected->source_index}) {
            selected = &candidate;
        }
    }

    return selected == nullptr ? std::nullopt : std::optional<std::size_t>{selected->source_index};
}

VkSurfaceFormatKHR choose_surface_format(const std::span<const VkSurfaceFormatKHR> formats) {
    if (formats.size() == 1U && formats.front().format == VK_FORMAT_UNDEFINED
        && formats.front().colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return {VK_FORMAT_B8G8R8A8_SRGB, formats.front().colorSpace};
    }

    constexpr std::array preferred_formats{
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
    };
    for (const VkFormat preferred : preferred_formats) {
        for (const VkSurfaceFormatKHR format : formats) {
            if (format.format == preferred
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
    }

    throw std::invalid_argument("Vulkan surface does not expose a supported sRGB/nonlinear format");
}

VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                                   const VkExtent2D drawable_extent) noexcept {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    return {
        std::clamp(drawable_extent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width),
        std::clamp(drawable_extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height),
    };
}

std::uint32_t choose_swapchain_image_count(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.maxImageCount != 0U
        && capabilities.maxImageCount < capabilities.minImageCount) {
        throw std::invalid_argument("Vulkan surface image limits are inconsistent");
    }

    std::uint32_t desired = capabilities.minImageCount;
    if (desired != std::numeric_limits<std::uint32_t>::max()) {
        ++desired;
    }
    if (capabilities.maxImageCount != 0U) {
        desired = std::min(desired, capabilities.maxImageCount);
    }
    return desired;
}

VkCompositeAlphaFlagBitsKHR choose_composite_alpha(const VkCompositeAlphaFlagsKHR supported) {
    constexpr std::array preferences{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (const VkCompositeAlphaFlagBitsKHR candidate : preferences) {
        if ((supported & candidate) != 0U) {
            return candidate;
        }
    }
    throw std::invalid_argument("Vulkan surface exposes no supported composite-alpha mode");
}

} // namespace game_ex::render::vulkan_detail
