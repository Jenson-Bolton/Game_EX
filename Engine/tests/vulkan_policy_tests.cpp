/**
 * @file vulkan_policy_tests.cpp
 * @brief Deterministic unit tests for private Vulkan selection policies.
 */

#include "render/vulkan/vulkan_policy.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

/**
 * @brief Reports one failed assertion without aborting remaining tests.
 * @param condition Assertion result.
 * @param description Human-readable assertion description.
 * @return The unchanged assertion result.
 */
bool check(const bool condition, const std::string_view description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
    }
    return condition;
}

/**
 * @brief Tests combined-family preference and deterministic separate fallback.
 * @return True when queue selection follows its documented stable order.
 */
bool test_queue_selection() {
    using game_ex::render::vulkan_detail::QueueFamilySupport;
    const std::array families{
        QueueFamilySupport{7U, true, false},
        QueueFamilySupport{4U, false, true},
        QueueFamilySupport{6U, true, true},
        QueueFamilySupport{2U, true, true},
    };
    const auto combined = game_ex::render::vulkan_detail::choose_queue_families(families);

    const std::array separate_families{
        QueueFamilySupport{9U, false, true},
        QueueFamilySupport{5U, true, false},
        QueueFamilySupport{3U, false, true},
        QueueFamilySupport{1U, true, false},
    };
    const auto separate = game_ex::render::vulkan_detail::choose_queue_families(separate_families);
    const std::array incomplete{QueueFamilySupport{0U, true, false}};

    bool passed = check(combined.has_value(), "combined queue family is found");
    passed &= check(combined.has_value() && combined->graphics_family == 2U
                        && combined->presentation_family == 2U,
                    "lowest combined queue family wins independently of input order");
    passed &= check(separate.has_value() && separate->graphics_family == 1U
                        && separate->presentation_family == 3U,
                    "lowest deterministic separate queue pair is selected");
    passed &= check(!game_ex::render::vulkan_detail::choose_queue_families(incomplete).has_value(),
                    "missing presentation support is rejected");
    return passed;
}

/**
 * @brief Builds a deterministic physical-device policy candidate.
 * @param source_index Stable native enumeration index.
 * @param type Vulkan device class.
 * @param uuid_first First UUID byte used as a stable tie-break.
 * @param suitable Whether the device met all backend requirements.
 * @return Fully populated policy candidate.
 */
game_ex::render::vulkan_detail::PhysicalDeviceCandidate candidate(const std::size_t source_index,
                                                                  const VkPhysicalDeviceType type,
                                                                  const std::uint8_t uuid_first,
                                                                  const bool suitable = true) {
    game_ex::render::vulkan_detail::PhysicalDeviceCandidate value{};
    value.source_index = source_index;
    value.type = type;
    value.uuid[0] = uuid_first;
    value.vendor_id = 10U;
    value.device_id = 20U;
    value.name = "fixture";
    value.suitable = suitable;
    return value;
}

/**
 * @brief Tests stable device preference under shuffled enumeration order.
 * @return True when class, suitability, and UUID policy remain deterministic.
 */
bool test_device_selection() {
    std::vector candidates{
        candidate(0U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 1U),
        candidate(1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 8U),
        candidate(2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 3U),
        candidate(3U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0U, false),
    };
    bool passed = true;
    do {
        const auto selected = game_ex::render::vulkan_detail::choose_physical_device(candidates);
        passed &= check(selected.has_value() && selected.value() == 2U,
                        "preferred physical device is independent of enumeration order");
    } while (std::next_permutation(candidates.begin(), candidates.end(),
                                   [](const auto& left, const auto& right) {
                                       return left.source_index < right.source_index;
                                   }));

    const std::array none{candidate(4U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 1U, false)};
    passed &= check(!game_ex::render::vulkan_detail::choose_physical_device(none).has_value(),
                    "an entirely unsuitable device set is rejected");
    return passed;
}

/**
 * @brief Tests strict sRGB/nonlinear surface-format policy.
 * @return True when preference and rejection cases are exact.
 */
bool test_surface_format_selection() {
    const std::array formats{
        VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    };
    const VkSurfaceFormatKHR selected =
        game_ex::render::vulkan_detail::choose_surface_format(formats);
    const std::array undefined{
        VkSurfaceFormatKHR{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    const VkSurfaceFormatKHR selected_undefined =
        game_ex::render::vulkan_detail::choose_surface_format(undefined);

    bool rejected_non_srgb = false;
    try {
        const std::array non_srgb{
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
        static_cast<void>(game_ex::render::vulkan_detail::choose_surface_format(non_srgb));
    } catch (const std::invalid_argument&) {
        rejected_non_srgb = true;
    }
    bool rejected_empty = false;
    try {
        const std::array<VkSurfaceFormatKHR, 0U> empty{};
        static_cast<void>(game_ex::render::vulkan_detail::choose_surface_format(empty));
    } catch (const std::invalid_argument&) {
        rejected_empty = true;
    }
    bool rejected_wrong_undefined_space = false;
    try {
        const std::array wrong_space{
            VkSurfaceFormatKHR{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT}};
        static_cast<void>(game_ex::render::vulkan_detail::choose_surface_format(wrong_space));
    } catch (const std::invalid_argument&) {
        rejected_wrong_undefined_space = true;
    }

    bool passed =
        check(selected.format == VK_FORMAT_B8G8R8A8_SRGB, "B8 sRGB is preferred over R8 sRGB");
    passed &= check(selected_undefined.format == VK_FORMAT_B8G8R8A8_SRGB,
                    "single undefined format resolves to preferred B8 sRGB");
    passed &= check(rejected_non_srgb, "non-sRGB fallback is forbidden");
    passed &= check(rejected_empty, "empty surface-format set is rejected");
    passed &= check(rejected_wrong_undefined_space,
                    "undefined format with a non-sRGB colour space is rejected");
    return passed;
}

/**
 * @brief Tests deterministic extent, image-count, and composite-alpha choices.
 * @return True when each swapchain policy obeys Vulkan capability bounds.
 */
bool test_swapchain_policies() {
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.currentExtent = {UINT32_MAX, UINT32_MAX};
    capabilities.minImageExtent = {100U, 80U};
    capabilities.maxImageExtent = {800U, 600U};
    capabilities.minImageCount = 2U;
    capabilities.maxImageCount = 3U;
    const VkExtent2D clamped =
        game_ex::render::vulkan_detail::choose_swapchain_extent(capabilities, {900U, 20U});
    VkSurfaceCapabilitiesKHR fixed_capabilities = capabilities;
    fixed_capabilities.currentExtent = {640U, 480U};
    const VkExtent2D fixed =
        game_ex::render::vulkan_detail::choose_swapchain_extent(fixed_capabilities, {1U, 1U});

    VkSurfaceCapabilitiesKHR unlimited = capabilities;
    unlimited.maxImageCount = 0U;
    VkSurfaceCapabilitiesKHR capped_at_minimum = capabilities;
    capped_at_minimum.maxImageCount = capped_at_minimum.minImageCount;
    VkSurfaceCapabilitiesKHR inconsistent = capabilities;
    inconsistent.minImageCount = 3U;
    inconsistent.maxImageCount = 2U;
    bool rejected_image_limits = false;
    try {
        static_cast<void>(
            game_ex::render::vulkan_detail::choose_swapchain_image_count(inconsistent));
    } catch (const std::invalid_argument&) {
        rejected_image_limits = true;
    }
    bool rejected_composite_alpha = false;
    try {
        static_cast<void>(game_ex::render::vulkan_detail::choose_composite_alpha(0U));
    } catch (const std::invalid_argument&) {
        rejected_composite_alpha = true;
    }

    bool passed = check(clamped.width == 800U && clamped.height == 80U,
                        "variable surface extent clamps both dimensions");
    passed &= check(fixed.width == 640U && fixed.height == 480U,
                    "fixed surface extent ignores the drawable request");
    passed &=
        check(game_ex::render::vulkan_detail::choose_swapchain_image_count(capabilities) == 3U,
              "image policy requests one more than the minimum within the maximum");
    passed &= check(game_ex::render::vulkan_detail::choose_swapchain_image_count(unlimited) == 3U,
                    "unlimited image maximum permits minimum plus one");
    passed &=
        check(game_ex::render::vulkan_detail::choose_swapchain_image_count(capped_at_minimum) == 2U,
              "image maximum equal to minimum remains valid");
    passed &= check(rejected_image_limits, "inconsistent image-count bounds are rejected");
    passed &=
        check(game_ex::render::vulkan_detail::choose_composite_alpha(
                  VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
                  == VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
              "composite-alpha preference is stable");
    passed &= check(rejected_composite_alpha, "missing composite-alpha support is rejected");
    return passed;
}

} // namespace

/**
 * @brief Runs all deterministic Vulkan policy tests.
 * @return EXIT_SUCCESS when all assertions pass.
 */
int main() {
    bool passed = true;
    passed &= test_queue_selection();
    passed &= test_device_selection();
    passed &= test_surface_format_selection();
    passed &= test_swapchain_policies();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
