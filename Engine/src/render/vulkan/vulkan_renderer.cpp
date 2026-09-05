/**
 * @file vulkan_renderer.cpp
 * @brief SDL/Vulkan 1.3 diagnostic-frame renderer implementation.
 */

#include "game_ex/render/vulkan_renderer.hpp"

#include "platform/sdl/sdl_window_access.hpp"
#include "render/diagnostic_raster_layout.hpp"
#include "vulkan_policy.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace game_ex::render {
namespace {

/** Vulkan API baseline required by ADR 0007. */
constexpr std::uint32_t required_api_version{VK_API_VERSION_1_3};

/** Khronos validation layer requested by validation-enabled configurations. */
constexpr std::string_view validation_layer_name{"VK_LAYER_KHRONOS_validation"};

/** Debug-utils extension used to observe validation errors. */
constexpr std::string_view debug_utils_extension_name{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

/** Portability-enumeration extension requiring a matching instance flag. */
constexpr std::string_view portability_enumeration_extension_name{
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};

/** Optional portability-subset device extension required when advertised. */
constexpr std::string_view portability_subset_extension_name{"VK_KHR_portability_subset"};

/** Maximum attempts made when a Vulkan enumeration changes concurrently. */
constexpr int maximum_enumeration_attempts{8};

/** Image uses required by both clear-only and raster presentations. */
constexpr VkImageUsageFlags required_swapchain_image_usage{
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};

/** Optimal-image features required by both diagnostic presentation paths. */
constexpr VkFormatFeatureFlags required_swapchain_format_features{
    VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT};

/**
 * @brief Converts a Vulkan result into stable diagnostic text.
 * @param result Vulkan result code.
 * @return Static symbolic name for known results.
 */
[[nodiscard]] std::string_view vulkan_result_name(const VkResult result) noexcept {
    switch (result) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    default:
        return "VK_RESULT_UNKNOWN";
    }
}

/**
 * @brief Builds a renderer error from a failed Vulkan operation.
 * @param code Stable backend-neutral category.
 * @param operation Human-readable operation description.
 * @param result Vulkan result returned by the operation.
 * @return Categorized exception ready to throw.
 */
[[nodiscard]] RendererError vulkan_error(const RendererErrorCode code,
                                         const std::string_view operation, const VkResult result) {
    std::ostringstream message;
    message << operation << " failed with " << vulkan_result_name(result) << " ("
            << static_cast<int>(result) << ')';
    return RendererError{code, RendererBackend::vulkan, message.str()};
}

/**
 * @brief Builds a renderer error with the current SDL diagnostic.
 * @param code Stable backend-neutral category.
 * @param operation Human-readable operation description.
 * @return Categorized exception ready to throw.
 */
[[nodiscard]] RendererError current_sdl_error(const RendererErrorCode code,
                                              const std::string_view operation) {
    return RendererError{code, RendererBackend::vulkan,
                         std::string{operation} + ": " + SDL_GetError()};
}

/**
 * @brief Converts a lifecycle state to stable diagnostic text.
 * @param state Lifecycle state to describe.
 * @return Static lowercase state name.
 */
[[nodiscard]] std::string_view lifecycle_state_name(const RendererLifecycleState state) noexcept {
    switch (state) {
    case RendererLifecycleState::dormant:
        return "dormant";
    case RendererLifecycleState::starting:
        return "starting";
    case RendererLifecycleState::running:
        return "running";
    case RendererLifecycleState::stopping:
        return "stopping";
    case RendererLifecycleState::stopped:
        return "stopped";
    case RendererLifecycleState::failed:
        return "failed";
    }
    return "unknown";
}

/**
 * @brief Converts a bounded container size to a Vulkan count.
 * @param size Native container size.
 * @param subject Diagnostic subject whose count is converted.
 * @return Count representable by Vulkan's 32-bit interfaces.
 * @throws RendererError when size exceeds the Vulkan count range.
 */
[[nodiscard]] std::uint32_t checked_vulkan_count(const std::size_t size,
                                                 const std::string_view subject) {
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw RendererError{RendererErrorCode::initialization_failed, RendererBackend::vulkan,
                            std::string{subject} + " exceeds the Vulkan 32-bit count range"};
    }
    return static_cast<std::uint32_t>(size);
}

/**
 * @brief Repeats a Vulkan count/data enumeration until it settles.
 * @tparam Value Enumerated Vulkan value type.
 * @tparam Enumerator Callable compatible with a count and optional value array.
 * @param enumerate Callable invoking the relevant Vulkan enumeration function.
 * @param operation Human-readable operation description.
 * @param error_code Stable failure category used when enumeration fails.
 * @return Settled values in Vulkan-provided order.
 * @throws RendererError if Vulkan fails or never produces a complete snapshot.
 */
template <typename Value, typename Enumerator>
[[nodiscard]] std::vector<Value>
enumerate_values(Enumerator&& enumerate, const std::string_view operation,
                 const RendererErrorCode error_code = RendererErrorCode::initialization_failed) {
    for (int attempt = 0; attempt < maximum_enumeration_attempts; ++attempt) {
        std::uint32_t count{};
        const VkResult count_result = enumerate(&count, nullptr);
        if (count_result != VK_SUCCESS && count_result != VK_INCOMPLETE) {
            throw vulkan_error(error_code, operation, count_result);
        }
        if (count == 0U) {
            return {};
        }

        std::vector<Value> values(count);
        std::uint32_t written = count;
        const VkResult values_result = enumerate(&written, values.data());
        if (values_result == VK_SUCCESS) {
            if (written > count) {
                throw RendererError{error_code, RendererBackend::vulkan,
                                    std::string{operation}
                                        + " returned more values than requested"};
            }
            values.resize(written);
            return values;
        }
        if (values_result != VK_INCOMPLETE) {
            throw vulkan_error(error_code, operation, values_result);
        }
    }

    throw RendererError{error_code, RendererBackend::vulkan,
                        std::string{operation} + " did not settle after bounded retries"};
}

/**
 * @brief Tests whether one named extension exists in enumerated properties.
 * @param extensions Vulkan extension properties.
 * @param required_name Null-terminated extension name.
 * @return True when an exact name match exists.
 */
[[nodiscard]] bool has_extension(const std::span<const VkExtensionProperties> extensions,
                                 const std::string_view required_name) noexcept {
    return std::any_of(extensions.begin(), extensions.end(),
                       [required_name](const VkExtensionProperties& extension) {
                           return required_name == extension.extensionName;
                       });
}

/**
 * @brief Tests whether one named layer exists in enumerated properties.
 * @param layers Vulkan instance-layer properties.
 * @param required_name Exact layer name.
 * @return True when an exact name match exists.
 */
[[nodiscard]] bool has_layer(const std::span<const VkLayerProperties> layers,
                             const std::string_view required_name) noexcept {
    return std::any_of(layers.begin(), layers.end(),
                       [required_name](const VkLayerProperties& layer) {
                           return required_name == layer.layerName;
                       });
}

/**
 * @brief Converts a Vulkan version value to dotted decimal text.
 * @param version Packed Vulkan API version.
 * @return Major.minor.patch version text.
 */
[[nodiscard]] std::string version_text(const std::uint32_t version) {
    return std::to_string(VK_API_VERSION_MAJOR(version)) + '.'
           + std::to_string(VK_API_VERSION_MINOR(version)) + '.'
           + std::to_string(VK_API_VERSION_PATCH(version));
}

/**
 * @brief Converts a Vulkan format to the stable names used by this backend.
 * @param format Selected Vulkan image format.
 * @return Static symbolic format name or an unknown marker.
 */
[[nodiscard]] std::string_view format_name(const VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB:
        return "VK_FORMAT_B8G8R8A8_SRGB";
    case VK_FORMAT_R8G8B8A8_SRGB:
        return "VK_FORMAT_R8G8B8A8_SRGB";
    default:
        return "VK_FORMAT_UNKNOWN";
    }
}

/**
 * @brief Converts one validated linear colour to Vulkan clear storage.
 * @param colour Backend-neutral linear RGBA colour.
 * @return Vulkan floating-point clear colour.
 */
[[nodiscard]] VkClearColorValue vulkan_clear_colour(const DiagnosticFrame& colour) noexcept {
    VkClearColorValue clear{};
    clear.float32[0] = colour.red;
    clear.float32[1] = colour.green;
    clear.float32[2] = colour.blue;
    clear.float32[3] = colour.alpha;
    return clear;
}

/**
 * @brief Thread-safe state written by Vulkan validation callbacks.
 */
struct ValidationState final {
    /** Number of error-severity validation messages observed. */
    std::atomic<std::uint64_t> error_count{};
};

/**
 * @brief Records and reports one Vulkan debug-utils message without throwing.
 * @param severity Vulkan message severity bits.
 * @param types Vulkan message category bits.
 * @param callback_data Driver-owned message details.
 * @param user_data Pointer to the owning ValidationState.
 * @return VK_FALSE so Vulkan never aborts the triggering API operation.
 */
VKAPI_ATTR VkBool32 VKAPI_CALL
validation_callback(const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                    const VkDebugUtilsMessageTypeFlagsEXT types,
                    const VkDebugUtilsMessengerCallbackDataEXT* const callback_data,
                    void* const user_data) noexcept {
    static_cast<void>(types);
    auto* const state = static_cast<ValidationState*>(user_data);
    if (state != nullptr && (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        state->error_count.fetch_add(1U, std::memory_order_relaxed);
    }

    const char* const message = callback_data != nullptr && callback_data->pMessage != nullptr
                                    ? callback_data->pMessage
                                    : "Vulkan validation emitted a message without text";
    static_cast<void>(std::fputs("Vulkan validation: ", stderr));
    static_cast<void>(std::fputs(message, stderr));
    static_cast<void>(std::fputc('\n', stderr));
    return VK_FALSE;
}

/**
 * @brief Creates the shared debug-utils messenger configuration.
 * @param validation_state Callback state that outlives the messenger.
 * @return Fully initialized messenger creation information.
 */
[[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT
debug_messenger_create_info(ValidationState& validation_state) noexcept {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = validation_callback;
    info.pUserData = &validation_state;
    return info;
}

/**
 * @brief All replaceable resources belonging to one swapchain generation.
 */
struct SwapchainState final {
    /** Native swapchain handle. */
    VkSwapchainKHR handle{VK_NULL_HANDLE};

    /** Selected sRGB image format. */
    VkFormat format{VK_FORMAT_UNDEFINED};

    /** Selected image colour space. */
    VkColorSpaceKHR color_space{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    /** Selected presentation extent in drawable pixels. */
    VkExtent2D extent{};

    /** Drawable extent that caused this generation to be built. */
    VkExtent2D requested_extent{};

    /** Images owned by the swapchain. */
    std::vector<VkImage> images;

    /** One colour-attachment view owned for each swapchain image. */
    std::vector<VkImageView> image_views;

    /** Whether each image has previously reached presentation layout. */
    std::vector<bool> initialized;

    /** One binary present-wait semaphore dedicated to each swapchain image. */
    std::vector<VkSemaphore> presentation_semaphores;
};

/**
 * @brief Evaluation facts retained while choosing a physical device.
 */
struct EvaluatedDevice final {
    /** Deterministic policy input. */
    vulkan_detail::PhysicalDeviceCandidate candidate;

    /** Native handle supplied by instance enumeration. */
    VkPhysicalDevice handle{VK_NULL_HANDLE};

    /** Physical-device properties used for diagnostics. */
    VkPhysicalDeviceProperties properties{};

    /** Required graphics and presentation queues, when available. */
    std::optional<vulkan_detail::QueueSelection> queues;

    /** Whether the optional portability-subset extension was advertised. */
    bool portability_subset{};
};

/**
 * @brief SDL-backed Vulkan 1.3 renderer with unique native ownership.
 */
class VulkanRenderer final : public Renderer {
public:
    /**
     * @brief Binds a dormant renderer to a checked SDL Vulkan window.
     * @param window Native SDL window owned by the containing Application.
     * @param configuration Validation policy copied from the factory.
     */
    VulkanRenderer(SDL_Window& window, const VulkanRendererConfiguration configuration) noexcept
        : window_(window), configuration_(configuration),
          creator_thread_(std::this_thread::get_id()) {}

    /**
     * @brief Releases remaining Vulkan resources or fails fast off-thread.
     */
    ~VulkanRenderer() override {
        if (std::this_thread::get_id() != creator_thread_) {
            std::terminate();
        }
        cleanup_noexcept();
    }

    /** @copydoc game_ex::render::Renderer::backend */
    [[nodiscard]] RendererBackend backend() const noexcept override {
        return RendererBackend::vulkan;
    }

    /** @copydoc game_ex::render::Renderer::state */
    [[nodiscard]] RendererLifecycleState state() const noexcept override {
        return state_.load(std::memory_order_acquire);
    }

    /** @copydoc game_ex::render::Renderer::start */
    void start() override {
        require_creator_thread("start");
        require_state(RendererLifecycleState::dormant, "start");
        state_.store(RendererLifecycleState::starting, std::memory_order_release);

        try {
            create_instance();
            create_surface();
            choose_device();
            create_device();
            create_command_state();
            create_frame_synchronization();
            static_cast<void>(recreate_swapchain(RendererErrorCode::initialization_failed));
            update_diagnostics();
            throw_if_validation_errors(RendererErrorCode::initialization_failed,
                                       "Vulkan startup validation");
            state_.store(RendererLifecycleState::running, std::memory_order_release);
        } catch (const RendererError& error) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            if (error.code() == RendererErrorCode::presentation_failed) {
                throw RendererError{RendererErrorCode::initialization_failed,
                                    RendererBackend::vulkan, error.what()};
            }
            throw;
        } catch (...) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            throw;
        }
    }

    /** @copydoc game_ex::render::Renderer::render_frame */
    [[nodiscard]] FramePresentationResult render_frame(const RenderFrame& frame) override {
        require_creator_thread("render a frame");
        require_state(RendererLifecycleState::running, "render a frame");
        validate_render_frame(frame);
        try {
            throw_if_validation_errors(RendererErrorCode::presentation_failed,
                                       "Vulkan pre-frame validation");

            const VkExtent2D drawable_extent = query_drawable_extent();
            if (drawable_extent.width == 0U || drawable_extent.height == 0U) {
                return FramePresentationResult::deferred_zero_extent;
            }

            if (swapchain_.handle == VK_NULL_HANDLE || swapchain_dirty_
                || drawable_extent.width != swapchain_.requested_extent.width
                || drawable_extent.height != swapchain_.requested_extent.height) {
                if (!recreate_swapchain()) {
                    return FramePresentationResult::deferred_zero_extent;
                }
            }

            std::optional<detail::DiagnosticRasterLayout> raster_layout;
            if (frame.raster.has_value()) {
                constexpr std::uint32_t maximum_render_offset{
                    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())};
                if (swapchain_.extent.width > maximum_render_offset
                    || swapchain_.extent.height > maximum_render_offset) {
                    throw RendererError{
                        RendererErrorCode::presentation_failed, RendererBackend::vulkan,
                        "Vulkan diagnostic raster extent exceeds signed render offsets"};
                }
                raster_layout = detail::layout_diagnostic_raster(
                    frame.raster.value(), swapchain_.extent.width, swapchain_.extent.height);
            }

            VkResult wait_result = vkWaitForFences(device_, 1U, &in_flight_fence_, VK_TRUE,
                                                   std::numeric_limits<std::uint64_t>::max());
            if (wait_result != VK_SUCCESS) {
                throw vulkan_error(RendererErrorCode::presentation_failed,
                                   "Waiting for the Vulkan frame fence", wait_result);
            }

            std::uint32_t image_index{};
            VkResult acquire_result = acquire_next_image(image_index);
            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                swapchain_dirty_ = true;
                if (!recreate_swapchain()) {
                    return FramePresentationResult::deferred_zero_extent;
                }
                acquire_result = acquire_next_image(image_index);
            }
            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                swapchain_dirty_ = true;
                return FramePresentationResult::deferred_surface_change;
            }
            const bool acquired_suboptimal = acquire_result == VK_SUBOPTIMAL_KHR;
            if (acquire_result != VK_SUCCESS && !acquired_suboptimal) {
                throw vulkan_error(RendererErrorCode::presentation_failed,
                                   "Acquiring a Vulkan swapchain image", acquire_result);
            }
            if (image_index >= swapchain_.images.size()) {
                throw RendererError{RendererErrorCode::presentation_failed, RendererBackend::vulkan,
                                    "Vulkan acquired an image index outside the active swapchain"};
            }

            record_frame_commands(image_index, frame, raster_layout);

            const VkResult reset_fence_result = vkResetFences(device_, 1U, &in_flight_fence_);
            if (reset_fence_result != VK_SUCCESS) {
                throw vulkan_error(RendererErrorCode::presentation_failed,
                                   "Resetting the Vulkan frame fence", reset_fence_result);
            }

            VkSemaphoreSubmitInfo wait_info{};
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            wait_info.semaphore = image_available_semaphore_;
            wait_info.stageMask = raster_layout.has_value()
                                      ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                                      : VK_PIPELINE_STAGE_2_TRANSFER_BIT;

            VkCommandBufferSubmitInfo command_info{};
            command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_info.commandBuffer = command_buffer_;

            VkSemaphoreSubmitInfo signal_info{};
            signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signal_info.semaphore = swapchain_.presentation_semaphores[image_index];
            signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

            VkSubmitInfo2 submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.waitSemaphoreInfoCount = 1U;
            submit_info.pWaitSemaphoreInfos = &wait_info;
            submit_info.commandBufferInfoCount = 1U;
            submit_info.pCommandBufferInfos = &command_info;
            submit_info.signalSemaphoreInfoCount = 1U;
            submit_info.pSignalSemaphoreInfos = &signal_info;

            const VkResult submit_result =
                vkQueueSubmit2(graphics_queue_, 1U, &submit_info, in_flight_fence_);
            if (submit_result != VK_SUCCESS) {
                throw vulkan_error(RendererErrorCode::presentation_failed,
                                   "Submitting the Vulkan diagnostic frame", submit_result);
            }
            swapchain_.initialized[image_index] = true;

            VkPresentInfoKHR present_info{};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1U;
            present_info.pWaitSemaphores = &swapchain_.presentation_semaphores[image_index];
            present_info.swapchainCount = 1U;
            present_info.pSwapchains = &swapchain_.handle;
            present_info.pImageIndices = &image_index;

            const VkResult present_result = vkQueuePresentKHR(presentation_queue_, &present_info);
            if (present_result == VK_ERROR_OUT_OF_DATE_KHR) {
                swapchain_dirty_ = true;
                throw_if_validation_errors(RendererErrorCode::presentation_failed,
                                           "Vulkan frame validation");
                return FramePresentationResult::deferred_surface_change;
            }
            if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
                throw vulkan_error(RendererErrorCode::presentation_failed,
                                   "Presenting the Vulkan diagnostic frame", present_result);
            }

            if (acquired_suboptimal || present_result == VK_SUBOPTIMAL_KHR) {
                swapchain_dirty_ = true;
            }
            ++diagnostics_.presented_frames;
            if (frame.raster.has_value()) {
                diagnostics_.last_presented_raster_columns = frame.raster->columns;
                diagnostics_.last_presented_raster_rows = frame.raster->rows;
                diagnostics_.last_presented_raster_cell_count =
                    static_cast<std::uint64_t>(frame.raster->linear_colours.size());
            } else {
                diagnostics_.last_presented_raster_columns = 0U;
                diagnostics_.last_presented_raster_rows = 0U;
                diagnostics_.last_presented_raster_cell_count = 0U;
            }
            diagnostics_.debug_error_count =
                validation_state_.error_count.load(std::memory_order_relaxed);
            throw_if_validation_errors(RendererErrorCode::presentation_failed,
                                       "Vulkan frame validation");
            return FramePresentationResult::presented;
        } catch (...) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            throw;
        }
    }

    /** @copydoc game_ex::render::Renderer::diagnostics */
    [[nodiscard]] const RendererDiagnostics& diagnostics() const override {
        require_creator_thread("read diagnostics");
        require_state(RendererLifecycleState::running, "read diagnostics");
        return diagnostics_;
    }

    /** @copydoc game_ex::render::Renderer::shutdown */
    void shutdown() override {
        require_creator_thread("shut down");
        const RendererLifecycleState current = state();
        if (current != RendererLifecycleState::running
            && current != RendererLifecycleState::failed) {
            throw RendererError{RendererErrorCode::invalid_state, RendererBackend::vulkan,
                                "Cannot shut down a Vulkan renderer in state "
                                    + std::string{lifecycle_state_name(current)}};
        }

        state_.store(RendererLifecycleState::stopping, std::memory_order_release);
        VkResult wait_result = VK_SUCCESS;
        if (device_ != VK_NULL_HANDLE) {
            wait_result = vkDeviceWaitIdle(device_);
        }
        cleanup_noexcept();
        const std::uint64_t validation_errors =
            validation_state_.error_count.load(std::memory_order_relaxed);

        if (wait_result != VK_SUCCESS) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            throw vulkan_error(RendererErrorCode::shutdown_failed,
                               "Waiting for the Vulkan device during shutdown", wait_result);
        }
        if (validation_enabled_ && validation_errors != 0U) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            throw RendererError{RendererErrorCode::shutdown_failed, RendererBackend::vulkan,
                                "Vulkan validation reported " + std::to_string(validation_errors)
                                    + " error message(s) before or during shutdown"};
        }
        state_.store(RendererLifecycleState::stopped, std::memory_order_release);
    }

private:
    /**
     * @brief Rejects API use away from the composition thread.
     * @param operation Human-readable operation used in the exception.
     * @throws RendererError when called from another thread.
     */
    void require_creator_thread(const std::string_view operation) const {
        if (std::this_thread::get_id() != creator_thread_) {
            throw RendererError{RendererErrorCode::invalid_state, RendererBackend::vulkan,
                                "Vulkan renderer may only " + std::string{operation}
                                    + " on its creator thread"};
        }
    }

    /**
     * @brief Requires one exact lifecycle state.
     * @param expected State required by the operation.
     * @param operation Human-readable operation used in the exception.
     * @throws RendererError when the current state differs.
     */
    void require_state(const RendererLifecycleState expected,
                       const std::string_view operation) const {
        const RendererLifecycleState current = state();
        if (current != expected) {
            throw RendererError{RendererErrorCode::invalid_state, RendererBackend::vulkan,
                                "Cannot " + std::string{operation} + " a Vulkan renderer in state "
                                    + std::string{lifecycle_state_name(current)}};
        }
    }

    /**
     * @brief Creates the Vulkan 1.3 instance and optional validation messenger.
     * @throws RendererError for loader, extension, layer, or instance failures.
     */
    void create_instance() {
        std::uint32_t loader_version = VK_API_VERSION_1_0;
        const auto enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
        if (enumerate_instance_version != nullptr) {
            const VkResult result = enumerate_instance_version(&loader_version);
            if (result != VK_SUCCESS) {
                throw vulkan_error(RendererErrorCode::unavailable,
                                   "Querying the Vulkan loader version", result);
            }
        }
        if (loader_version < required_api_version) {
            throw RendererError{RendererErrorCode::unavailable, RendererBackend::vulkan,
                                "Vulkan loader " + version_text(loader_version)
                                    + " does not meet the Vulkan 1.3 baseline"};
        }
        loader_api_version_ = loader_version;

        Uint32 sdl_extension_count{};
        const char* const* const sdl_extensions =
            SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
        if (sdl_extensions == nullptr || sdl_extension_count == 0U) {
            throw current_sdl_error(RendererErrorCode::unavailable,
                                    "SDL could not provide required Vulkan instance extensions");
        }

        std::vector<std::string> enabled_extensions;
        enabled_extensions.reserve(static_cast<std::size_t>(sdl_extension_count) + 1U);
        for (Uint32 index = 0U; index < sdl_extension_count; ++index) {
            if (sdl_extensions[index] == nullptr || sdl_extensions[index][0] == '\0') {
                throw RendererError{RendererErrorCode::initialization_failed,
                                    RendererBackend::vulkan,
                                    "SDL returned an empty Vulkan instance extension name"};
            }
            enabled_extensions.emplace_back(sdl_extensions[index]);
        }

        const std::vector<VkExtensionProperties> available_extensions =
            enumerate_values<VkExtensionProperties>(
                [](std::uint32_t* count, VkExtensionProperties* values) {
                    return vkEnumerateInstanceExtensionProperties(nullptr, count, values);
                },
                "Enumerating Vulkan instance extensions", RendererErrorCode::unavailable);
        for (const std::string& extension : enabled_extensions) {
            if (!has_extension(available_extensions, extension)) {
                throw RendererError{RendererErrorCode::unavailable, RendererBackend::vulkan,
                                    "Required SDL Vulkan instance extension is unavailable: "
                                        + extension};
            }
        }

        const std::vector<VkLayerProperties> available_layers = enumerate_values<VkLayerProperties>(
            [](std::uint32_t* count, VkLayerProperties* values) {
                return vkEnumerateInstanceLayerProperties(count, values);
            },
            "Enumerating Vulkan instance layers", RendererErrorCode::unavailable);
        const bool validation_layer_available = has_layer(available_layers, validation_layer_name);
        const bool debug_utils_available =
            has_extension(available_extensions, debug_utils_extension_name);
        if (configuration_.validation == VulkanValidationMode::required
            && (!validation_layer_available || !debug_utils_available)) {
            throw RendererError{RendererErrorCode::unavailable, RendererBackend::vulkan,
                                "Required Vulkan validation layer or "
                                "VK_EXT_debug_utils is unavailable"};
        }
        validation_enabled_ = configuration_.validation != VulkanValidationMode::disabled
                              && validation_layer_available && debug_utils_available;
        if (validation_enabled_
            && std::find(enabled_extensions.begin(), enabled_extensions.end(),
                         debug_utils_extension_name)
                   == enabled_extensions.end()) {
            enabled_extensions.emplace_back(debug_utils_extension_name);
        }

        std::vector<const char*> extension_names;
        extension_names.reserve(enabled_extensions.size());
        for (const std::string& extension : enabled_extensions) {
            extension_names.push_back(extension.c_str());
        }

        const bool portability_enumeration = std::any_of(
            enabled_extensions.begin(), enabled_extensions.end(), [](const std::string& extension) {
                return extension == portability_enumeration_extension_name;
            });

        VkApplicationInfo application_info{};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "Game_EX";
        application_info.applicationVersion = VK_MAKE_API_VERSION(
            0, GAMEEX_VERSION_MAJOR, GAMEEX_VERSION_MINOR, GAMEEX_VERSION_PATCH);
        application_info.pEngineName = "Game_EX Engine";
        application_info.engineVersion = VK_MAKE_API_VERSION(
            0, GAMEEX_VERSION_MAJOR, GAMEEX_VERSION_MINOR, GAMEEX_VERSION_PATCH);
        application_info.apiVersion = required_api_version;

        const char* validation_layer = validation_layer_name.data();
        VkDebugUtilsMessengerCreateInfoEXT debug_info =
            debug_messenger_create_info(validation_state_);
        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pNext = validation_enabled_ ? &debug_info : nullptr;
        instance_info.flags =
            portability_enumeration ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0U;
        instance_info.pApplicationInfo = &application_info;
        instance_info.enabledLayerCount = validation_enabled_ ? 1U : 0U;
        instance_info.ppEnabledLayerNames = validation_enabled_ ? &validation_layer : nullptr;
        instance_info.enabledExtensionCount =
            checked_vulkan_count(extension_names.size(), "Enabled Vulkan instance extensions");
        instance_info.ppEnabledExtensionNames = extension_names.data();

        VkInstance created_instance{VK_NULL_HANDLE};
        const VkResult create_result = vkCreateInstance(&instance_info, nullptr, &created_instance);
        if (create_result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::unavailable, "Creating the Vulkan 1.3 instance",
                               create_result);
        }
        instance_ = created_instance;

        if (validation_enabled_) {
            create_debug_messenger_ = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
            destroy_debug_messenger_ = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
            if (create_debug_messenger_ == nullptr || destroy_debug_messenger_ == nullptr) {
                throw RendererError{
                    RendererErrorCode::initialization_failed, RendererBackend::vulkan,
                    "Vulkan did not resolve required debug-utils messenger functions"};
            }
            VkDebugUtilsMessengerEXT created_messenger{VK_NULL_HANDLE};
            const VkResult debug_result =
                create_debug_messenger_(instance_, &debug_info, nullptr, &created_messenger);
            if (debug_result != VK_SUCCESS) {
                throw vulkan_error(RendererErrorCode::initialization_failed,
                                   "Creating the Vulkan validation messenger", debug_result);
            }
            debug_messenger_ = created_messenger;
        }
    }

    /**
     * @brief Creates the SDL-owned Vulkan presentation surface.
     * @throws RendererError when SDL cannot create the surface.
     */
    void create_surface() {
        VkSurfaceKHR created_surface{VK_NULL_HANDLE};
        if (!SDL_Vulkan_CreateSurface(&window_, instance_, nullptr, &created_surface)) {
            throw current_sdl_error(RendererErrorCode::initialization_failed,
                                    "SDL could not create the Vulkan presentation surface");
        }
        surface_ = created_surface;
    }

    /**
     * @brief Evaluates and deterministically selects one suitable physical
     * device.
     * @throws RendererError when no Vulkan 1.3 diagnostic-presentation device
     * exists.
     */
    void choose_device() {
        const std::vector<VkPhysicalDevice> physical_devices = enumerate_values<VkPhysicalDevice>(
            [this](std::uint32_t* count, VkPhysicalDevice* values) {
                return vkEnumeratePhysicalDevices(instance_, count, values);
            },
            "Enumerating Vulkan physical devices", RendererErrorCode::unavailable);
        if (physical_devices.empty()) {
            throw RendererError{RendererErrorCode::unavailable, RendererBackend::vulkan,
                                "Vulkan reported no physical devices"};
        }

        std::vector<EvaluatedDevice> evaluated;
        evaluated.reserve(physical_devices.size());
        std::vector<vulkan_detail::PhysicalDeviceCandidate> policy_candidates;
        policy_candidates.reserve(physical_devices.size());

        for (std::size_t index = 0; index < physical_devices.size(); ++index) {
            const VkPhysicalDevice handle = physical_devices[index];
            VkPhysicalDeviceIDProperties identity{};
            identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext = &identity;
            vkGetPhysicalDeviceProperties2(handle, &properties2);

            VkPhysicalDeviceVulkan13Features features13{};
            features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &features13;
            vkGetPhysicalDeviceFeatures2(handle, &features2);

            const std::vector<VkExtensionProperties> extensions =
                enumerate_values<VkExtensionProperties>(
                    [handle](std::uint32_t* count, VkExtensionProperties* values) {
                        return vkEnumerateDeviceExtensionProperties(handle, nullptr, count, values);
                    },
                    "Enumerating Vulkan device extensions", RendererErrorCode::unavailable);

            std::uint32_t queue_count{};
            vkGetPhysicalDeviceQueueFamilyProperties(handle, &queue_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
            if (queue_count != 0U) {
                vkGetPhysicalDeviceQueueFamilyProperties(handle, &queue_count,
                                                         queue_properties.data());
                queue_properties.resize(queue_count);
            }
            std::vector<vulkan_detail::QueueFamilySupport> queue_support;
            queue_support.reserve(queue_properties.size());
            for (std::size_t queue_index = 0; queue_index < queue_properties.size();
                 ++queue_index) {
                VkBool32 presentation_supported = VK_FALSE;
                const VkResult support_result = vkGetPhysicalDeviceSurfaceSupportKHR(
                    handle, checked_vulkan_count(queue_index, "Vulkan queue-family index"),
                    surface_, &presentation_supported);
                if (support_result != VK_SUCCESS) {
                    throw vulkan_error(RendererErrorCode::initialization_failed,
                                       "Querying Vulkan queue presentation support",
                                       support_result);
                }
                queue_support.push_back({
                    .index = checked_vulkan_count(queue_index, "Vulkan queue-family index"),
                    .graphics =
                        (queue_properties[queue_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U,
                    .presentation = presentation_supported == VK_TRUE,
                });
            }

            VkSurfaceCapabilitiesKHR capabilities{};
            const VkResult capabilities_result =
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(handle, surface_, &capabilities);
            if (capabilities_result != VK_SUCCESS) {
                throw vulkan_error(RendererErrorCode::initialization_failed,
                                   "Querying Vulkan surface capabilities", capabilities_result);
            }
            const std::vector<VkSurfaceFormatKHR> formats =
                surface_formats(handle, RendererErrorCode::unavailable);
            const std::vector<VkPresentModeKHR> present_modes =
                presentation_modes(handle, RendererErrorCode::unavailable);
            bool has_required_srgb_format_features{};
            try {
                const VkSurfaceFormatKHR chosen_format =
                    vulkan_detail::choose_surface_format(formats);
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(handle, chosen_format.format, &properties);
                has_required_srgb_format_features =
                    (properties.optimalTilingFeatures & required_swapchain_format_features)
                    == required_swapchain_format_features;
            } catch (const std::invalid_argument&) {
                has_required_srgb_format_features = false;
            }
            const bool has_fifo =
                std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_FIFO_KHR)
                != present_modes.end();
            const std::optional<vulkan_detail::QueueSelection> queues =
                vulkan_detail::choose_queue_families(queue_support);

            EvaluatedDevice device{};
            device.candidate.source_index = index;
            device.candidate.type = properties2.properties.deviceType;
            std::copy(std::begin(identity.deviceUUID), std::end(identity.deviceUUID),
                      device.candidate.uuid.begin());
            device.candidate.vendor_id = properties2.properties.vendorID;
            device.candidate.device_id = properties2.properties.deviceID;
            device.candidate.name = properties2.properties.deviceName;
            device.candidate.suitable =
                properties2.properties.apiVersion >= required_api_version
                && features13.synchronization2 == VK_TRUE
                && features13.dynamicRendering == VK_TRUE
                && has_extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME) && queues.has_value()
                && !formats.empty() && has_required_srgb_format_features && has_fifo
                && (capabilities.supportedUsageFlags & required_swapchain_image_usage)
                       == required_swapchain_image_usage;
            device.handle = handle;
            device.properties = properties2.properties;
            device.queues = queues;
            device.portability_subset =
                has_extension(extensions, portability_subset_extension_name);
            policy_candidates.push_back(device.candidate);
            evaluated.push_back(std::move(device));
        }

        const std::optional<std::size_t> selected_index =
            vulkan_detail::choose_physical_device(policy_candidates);
        if (!selected_index.has_value() || selected_index.value() >= evaluated.size()) {
            throw RendererError{RendererErrorCode::unavailable, RendererBackend::vulkan,
                                "No physical device satisfies Vulkan 1.3, synchronization2, "
                                "dynamic rendering, sRGB FIFO transfer/colour-attachment "
                                "presentation, and queue requirements"};
        }

        const EvaluatedDevice& selected = evaluated[selected_index.value()];
        physical_device_ = selected.handle;
        physical_device_properties_ = selected.properties;
        queue_selection_ = selected.queues.value();
        portability_subset_enabled_ = selected.portability_subset;
    }

    /**
     * @brief Creates the logical device and selected graphics/presentation
     * queues.
     * @throws RendererError when device creation fails.
     */
    void create_device() {
        constexpr float queue_priority{1.0F};
        std::array<VkDeviceQueueCreateInfo, 2U> queue_infos{};
        std::array<std::uint32_t, 2U> queue_families{queue_selection_.graphics_family,
                                                     queue_selection_.presentation_family};
        const std::size_t queue_info_count = queue_families[0] == queue_families[1] ? 1U : 2U;
        for (std::size_t index = 0; index < queue_info_count; ++index) {
            queue_infos[index].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_infos[index].queueFamilyIndex = queue_families[index];
            queue_infos[index].queueCount = 1U;
            queue_infos[index].pQueuePriorities = &queue_priority;
        }

        std::array<const char*, 2U> extension_names{VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                    portability_subset_extension_name.data()};
        const std::uint32_t extension_count = portability_subset_enabled_ ? 2U : 1U;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.synchronization2 = VK_TRUE;
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pNext = &features13;
        device_info.queueCreateInfoCount =
            checked_vulkan_count(queue_info_count, "Vulkan device queue descriptions");
        device_info.pQueueCreateInfos = queue_infos.data();
        device_info.enabledExtensionCount = extension_count;
        device_info.ppEnabledExtensionNames = extension_names.data();

        VkDevice created_device{VK_NULL_HANDLE};
        const VkResult result =
            vkCreateDevice(physical_device_, &device_info, nullptr, &created_device);
        if (result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::initialization_failed,
                               "Creating the Vulkan logical device", result);
        }
        device_ = created_device;
        vkGetDeviceQueue(device_, queue_selection_.graphics_family, 0U, &graphics_queue_);
        vkGetDeviceQueue(device_, queue_selection_.presentation_family, 0U, &presentation_queue_);
        if (graphics_queue_ == VK_NULL_HANDLE || presentation_queue_ == VK_NULL_HANDLE) {
            throw RendererError{RendererErrorCode::initialization_failed, RendererBackend::vulkan,
                                "Vulkan returned a null required device queue"};
        }
    }

    /**
     * @brief Creates the resettable graphics command pool and one command buffer.
     * @throws RendererError when command-state allocation fails.
     */
    void create_command_state() {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_selection_.graphics_family;
        VkCommandPool created_pool{VK_NULL_HANDLE};
        const VkResult pool_result =
            vkCreateCommandPool(device_, &pool_info, nullptr, &created_pool);
        if (pool_result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::initialization_failed,
                               "Creating the Vulkan command pool", pool_result);
        }
        command_pool_ = created_pool;

        VkCommandBufferAllocateInfo allocation_info{};
        allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocation_info.commandPool = command_pool_;
        allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation_info.commandBufferCount = 1U;
        VkCommandBuffer allocated_buffer{VK_NULL_HANDLE};
        const VkResult allocation_result =
            vkAllocateCommandBuffers(device_, &allocation_info, &allocated_buffer);
        if (allocation_result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::initialization_failed,
                               "Allocating the Vulkan command buffer", allocation_result);
        }
        command_buffer_ = allocated_buffer;
    }

    /**
     * @brief Creates one-frame-in-flight semaphores and an initially signalled
     * fence.
     * @throws RendererError when synchronization-object creation fails.
     */
    void create_frame_synchronization() {
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore created_image_available{VK_NULL_HANDLE};
        VkResult result =
            vkCreateSemaphore(device_, &semaphore_info, nullptr, &created_image_available);
        if (result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::initialization_failed,
                               "Creating the Vulkan image-available semaphore", result);
        }
        image_available_semaphore_ = created_image_available;
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence created_fence{VK_NULL_HANDLE};
        result = vkCreateFence(device_, &fence_info, nullptr, &created_fence);
        if (result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::initialization_failed,
                               "Creating the Vulkan frame fence", result);
        }
        in_flight_fence_ = created_fence;
    }

    /**
     * @brief Queries the current SDL drawable extent without leaking SDL
     * publicly.
     * @return Non-negative drawable pixel extent.
     * @param error_code Failure category appropriate to the calling lifecycle
     * phase.
     * @throws RendererError when SDL fails or returns a negative extent.
     */
    [[nodiscard]] VkExtent2D query_drawable_extent(
        const RendererErrorCode error_code = RendererErrorCode::presentation_failed) const {
        int width{};
        int height{};
        if (!SDL_GetWindowSizeInPixels(&window_, &width, &height)) {
            throw current_sdl_error(error_code, "SDL could not query the Vulkan drawable extent");
        }
        if (width < 0 || height < 0) {
            throw RendererError{error_code, RendererBackend::vulkan,
                                "SDL returned a negative Vulkan drawable extent"};
        }
        return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
    }

    /**
     * @brief Enumerates surface formats for one physical device.
     * @param device Physical device queried against the owned surface.
     * @param error_code Failure category appropriate to the calling lifecycle
     * phase.
     * @return Settled supported surface formats.
     * @throws RendererError when the query fails.
     */
    [[nodiscard]] std::vector<VkSurfaceFormatKHR>
    surface_formats(const VkPhysicalDevice device, const RendererErrorCode error_code) const {
        return enumerate_values<VkSurfaceFormatKHR>(
            [this, device](std::uint32_t* count, VkSurfaceFormatKHR* values) {
                return vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, count, values);
            },
            "Enumerating Vulkan surface formats", error_code);
    }

    /**
     * @brief Enumerates presentation modes for one physical device.
     * @param device Physical device queried against the owned surface.
     * @param error_code Failure category appropriate to the calling lifecycle
     * phase.
     * @return Settled supported presentation modes.
     * @throws RendererError when the query fails.
     */
    [[nodiscard]] std::vector<VkPresentModeKHR>
    presentation_modes(const VkPhysicalDevice device, const RendererErrorCode error_code) const {
        return enumerate_values<VkPresentModeKHR>(
            [this, device](std::uint32_t* count, VkPresentModeKHR* values) {
                return vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, count, values);
            },
            "Enumerating Vulkan presentation modes", error_code);
    }

    /**
     * @brief Destroys one swapchain generation in dependency-safe order.
     * @param swapchain Generation whose views, semaphores, and handle are released.
     */
    void destroy_swapchain_state(SwapchainState& swapchain) const noexcept {
        for (const VkImageView view : swapchain.image_views) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, view, nullptr);
            }
        }
        for (const VkSemaphore semaphore : swapchain.presentation_semaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_, semaphore, nullptr);
            }
        }
        if (swapchain.handle != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain.handle, nullptr);
        }
        swapchain = {};
    }

    /**
     * @brief Transactionally creates or replaces the active swapchain generation.
     * @param error_code Failure category appropriate to the calling lifecycle
     * phase.
     * @return False only when a zero drawable extent requires safe deferral.
     * @throws RendererError when a non-zero swapchain cannot be created.
     */
    [[nodiscard]] bool recreate_swapchain(
        const RendererErrorCode error_code = RendererErrorCode::presentation_failed) {
        const VkExtent2D drawable_extent = query_drawable_extent(error_code);
        if (drawable_extent.width == 0U || drawable_extent.height == 0U) {
            return false;
        }

        if (swapchain_.handle != VK_NULL_HANDLE) {
            const VkResult idle_result = vkDeviceWaitIdle(device_);
            if (idle_result != VK_SUCCESS) {
                throw vulkan_error(error_code, "Waiting for Vulkan swapchain recreation",
                                   idle_result);
            }
        }

        VkSurfaceCapabilitiesKHR capabilities{};
        const VkResult capabilities_result =
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities);
        if (capabilities_result != VK_SUCCESS) {
            throw vulkan_error(error_code, "Querying Vulkan swapchain capabilities",
                               capabilities_result);
        }
        if ((capabilities.supportedUsageFlags & required_swapchain_image_usage)
            != required_swapchain_image_usage) {
            throw RendererError{error_code, RendererBackend::vulkan,
                                "Vulkan surface does not support transfer-destination and "
                                "colour-attachment swapchain images"};
        }

        const std::vector<VkSurfaceFormatKHR> formats =
            surface_formats(physical_device_, error_code);
        const std::vector<VkPresentModeKHR> present_modes =
            presentation_modes(physical_device_, error_code);
        if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_FIFO_KHR)
            == present_modes.end()) {
            throw RendererError{error_code, RendererBackend::vulkan,
                                "Vulkan surface does not support required FIFO presentation"};
        }

        VkSurfaceFormatKHR selected_format{};
        VkCompositeAlphaFlagBitsKHR composite_alpha{};
        std::uint32_t image_count{};
        try {
            selected_format = vulkan_detail::choose_surface_format(formats);
            composite_alpha =
                vulkan_detail::choose_composite_alpha(capabilities.supportedCompositeAlpha);
            image_count = vulkan_detail::choose_swapchain_image_count(capabilities);
        } catch (const std::invalid_argument& error) {
            throw RendererError{error_code, RendererBackend::vulkan, error.what()};
        }
        VkFormatProperties selected_format_properties{};
        vkGetPhysicalDeviceFormatProperties(physical_device_, selected_format.format,
                                            &selected_format_properties);
        if ((selected_format_properties.optimalTilingFeatures
             & required_swapchain_format_features)
            != required_swapchain_format_features) {
            throw RendererError{error_code, RendererBackend::vulkan,
                                "Selected Vulkan sRGB swapchain format lacks required "
                                "transfer-destination or colour-attachment support"};
        }
        const VkExtent2D selected_extent =
            vulkan_detail::choose_swapchain_extent(capabilities, drawable_extent);
        if (selected_extent.width == 0U || selected_extent.height == 0U) {
            return false;
        }

        std::array queue_family_indices{queue_selection_.graphics_family,
                                        queue_selection_.presentation_family};
        const bool separate_queues = queue_family_indices[0] != queue_family_indices[1];

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface_;
        create_info.minImageCount = image_count;
        create_info.imageFormat = selected_format.format;
        create_info.imageColorSpace = selected_format.colorSpace;
        create_info.imageExtent = selected_extent;
        create_info.imageArrayLayers = 1U;
        create_info.imageUsage = required_swapchain_image_usage;
        create_info.imageSharingMode =
            separate_queues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = separate_queues ? 2U : 0U;
        create_info.pQueueFamilyIndices = separate_queues ? queue_family_indices.data() : nullptr;
        create_info.preTransform = capabilities.currentTransform;
        create_info.compositeAlpha = composite_alpha;
        create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = swapchain_.handle;

        SwapchainState replacement{};
        VkSwapchainKHR created_swapchain{VK_NULL_HANDLE};
        const VkResult create_result =
            vkCreateSwapchainKHR(device_, &create_info, nullptr, &created_swapchain);
        if (create_result != VK_SUCCESS) {
            throw vulkan_error(error_code, "Creating the Vulkan swapchain", create_result);
        }
        replacement.handle = created_swapchain;

        try {
            replacement.images = enumerate_values<VkImage>(
                [this, &replacement](std::uint32_t* count, VkImage* values) {
                    return vkGetSwapchainImagesKHR(device_, replacement.handle, count, values);
                },
                "Enumerating Vulkan swapchain images", error_code);
            if (replacement.images.empty()) {
                throw RendererError{error_code, RendererBackend::vulkan,
                                    "Vulkan created a swapchain without images"};
            }
            replacement.format = selected_format.format;
            replacement.color_space = selected_format.colorSpace;
            replacement.extent = selected_extent;
            replacement.requested_extent = drawable_extent;

            replacement.image_views.assign(replacement.images.size(), VK_NULL_HANDLE);
            for (std::size_t index = 0U; index < replacement.images.size(); ++index) {
                VkImageViewCreateInfo view_info{};
                view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                view_info.image = replacement.images[index];
                view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view_info.format = replacement.format;
                view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                view_info.subresourceRange.baseMipLevel = 0U;
                view_info.subresourceRange.levelCount = 1U;
                view_info.subresourceRange.baseArrayLayer = 0U;
                view_info.subresourceRange.layerCount = 1U;

                VkImageView created_view{VK_NULL_HANDLE};
                const VkResult view_result =
                    vkCreateImageView(device_, &view_info, nullptr, &created_view);
                if (view_result != VK_SUCCESS) {
                    throw vulkan_error(error_code,
                                       "Creating a Vulkan swapchain image view", view_result);
                }
                replacement.image_views[index] = created_view;
            }

            replacement.initialized.assign(replacement.images.size(), false);
            replacement.presentation_semaphores.assign(replacement.images.size(), VK_NULL_HANDLE);
            VkSemaphoreCreateInfo semaphore_info{};
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            for (VkSemaphore& semaphore : replacement.presentation_semaphores) {
                VkSemaphore created_semaphore{VK_NULL_HANDLE};
                const VkResult semaphore_result =
                    vkCreateSemaphore(device_, &semaphore_info, nullptr, &created_semaphore);
                if (semaphore_result != VK_SUCCESS) {
                    throw vulkan_error(error_code,
                                       "Creating a per-image Vulkan presentation semaphore",
                                       semaphore_result);
                }
                semaphore = created_semaphore;
            }
        } catch (...) {
            destroy_swapchain_state(replacement);
            throw;
        }

        SwapchainState old_swapchain = std::move(swapchain_);
        swapchain_ = std::move(replacement);
        destroy_swapchain_state(old_swapchain);
        swapchain_dirty_ = false;
        update_presentation_diagnostics();
        return true;
    }

    /**
     * @brief Acquires one active swapchain image for transfer clearing.
     * @param image_index Receives the acquired image index.
     * @return Vulkan success, suboptimal, out-of-date, or another checked result.
     */
    [[nodiscard]] VkResult acquire_next_image(std::uint32_t& image_index) const noexcept {
        return vkAcquireNextImageKHR(device_, swapchain_.handle,
                                     std::numeric_limits<std::uint64_t>::max(),
                                     image_available_semaphore_, VK_NULL_HANDLE, &image_index);
    }

    /**
     * @brief Records the transfer-only background clear presentation path.
     * @param image_index Active swapchain image index.
     * @param background Validated linear background colour.
     */
    void record_background_clear(const std::uint32_t image_index,
                                 const DiagnosticFrame& background) const noexcept {
        VkImageMemoryBarrier2 to_transfer{};
        to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        to_transfer.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_transfer.srcAccessMask = VK_ACCESS_2_NONE;
        to_transfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        to_transfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        to_transfer.oldLayout = swapchain_.initialized[image_index]
                                    ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                    : VK_IMAGE_LAYOUT_UNDEFINED;
        to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.image = swapchain_.images[image_index];
        to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_transfer.subresourceRange.baseMipLevel = 0U;
        to_transfer.subresourceRange.levelCount = 1U;
        to_transfer.subresourceRange.baseArrayLayer = 0U;
        to_transfer.subresourceRange.layerCount = 1U;

        VkDependencyInfo to_transfer_dependency{};
        to_transfer_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        to_transfer_dependency.imageMemoryBarrierCount = 1U;
        to_transfer_dependency.pImageMemoryBarriers = &to_transfer;
        vkCmdPipelineBarrier2(command_buffer_, &to_transfer_dependency);

        const VkClearColorValue clear = vulkan_clear_colour(background);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U};
        vkCmdClearColorImage(command_buffer_, swapchain_.images[image_index],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1U, &range);

        VkImageMemoryBarrier2 to_present{};
        to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        to_present.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        to_present.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        to_present.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_present.dstAccessMask = VK_ACCESS_2_NONE;
        to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.image = swapchain_.images[image_index];
        to_present.subresourceRange = range;

        VkDependencyInfo to_present_dependency{};
        to_present_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        to_present_dependency.imageMemoryBarrierCount = 1U;
        to_present_dependency.pImageMemoryBarriers = &to_present;
        vkCmdPipelineBarrier2(command_buffer_, &to_present_dependency);
    }

    /**
     * @brief Records one dynamic-rendering background plus raster presentation.
     * @param image_index Active swapchain image index.
     * @param frame Validated background and raster colours.
     * @param layout Shared bottom-origin integer raster layout.
     */
    void record_raster_clear(const std::uint32_t image_index, const RenderFrame& frame,
                             const detail::DiagnosticRasterLayout& layout) const noexcept {
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U};
        VkImageMemoryBarrier2 to_attachment{};
        to_attachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        to_attachment.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_attachment.srcAccessMask = VK_ACCESS_2_NONE;
        to_attachment.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        to_attachment.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        to_attachment.oldLayout = swapchain_.initialized[image_index]
                                      ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                      : VK_IMAGE_LAYOUT_UNDEFINED;
        to_attachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        to_attachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_attachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_attachment.image = swapchain_.images[image_index];
        to_attachment.subresourceRange = range;

        VkDependencyInfo to_attachment_dependency{};
        to_attachment_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        to_attachment_dependency.imageMemoryBarrierCount = 1U;
        to_attachment_dependency.pImageMemoryBarriers = &to_attachment;
        vkCmdPipelineBarrier2(command_buffer_, &to_attachment_dependency);

        VkClearValue background_clear{};
        background_clear.color = vulkan_clear_colour(frame.background);
        VkRenderingAttachmentInfo colour_attachment{};
        colour_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colour_attachment.imageView = swapchain_.image_views[image_index];
        colour_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colour_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colour_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colour_attachment.clearValue = background_clear;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = {0, 0};
        rendering_info.renderArea.extent = swapchain_.extent;
        rendering_info.layerCount = 1U;
        rendering_info.colorAttachmentCount = 1U;
        rendering_info.pColorAttachments = &colour_attachment;
        vkCmdBeginRendering(command_buffer_, &rendering_info);

        const DiagnosticRaster& raster = *frame.raster;
        for (const detail::DiagnosticRasterRectangle& cell : layout.cells) {
            if (cell.width == 0U || cell.height == 0U) {
                continue;
            }

            VkClearAttachment clear_attachment{};
            clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clear_attachment.colorAttachment = 0U;
            clear_attachment.clearValue.color =
                vulkan_clear_colour(raster.linear_colours[cell.colour_index]);

            // Shared row zero starts at the lower edge; Vulkan clear rectangles
            // measure their Y offset from the upper edge of the attachment.
            const std::uint32_t top_origin_y =
                swapchain_.extent.height - (cell.y + cell.height);
            VkClearRect clear_rectangle{};
            clear_rectangle.rect.offset.x = static_cast<std::int32_t>(cell.x);
            clear_rectangle.rect.offset.y = static_cast<std::int32_t>(top_origin_y);
            clear_rectangle.rect.extent.width = cell.width;
            clear_rectangle.rect.extent.height = cell.height;
            clear_rectangle.baseArrayLayer = 0U;
            clear_rectangle.layerCount = 1U;
            vkCmdClearAttachments(command_buffer_, 1U, &clear_attachment, 1U,
                                  &clear_rectangle);
        }

        vkCmdEndRendering(command_buffer_);

        VkImageMemoryBarrier2 to_present{};
        to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        to_present.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        to_present.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        to_present.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_present.dstAccessMask = VK_ACCESS_2_NONE;
        to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.image = swapchain_.images[image_index];
        to_present.subresourceRange = range;

        VkDependencyInfo to_present_dependency{};
        to_present_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        to_present_dependency.imageMemoryBarrierCount = 1U;
        to_present_dependency.pImageMemoryBarriers = &to_present;
        vkCmdPipelineBarrier2(command_buffer_, &to_present_dependency);
    }

    /**
     * @brief Records one complete validated diagnostic frame command buffer.
     * @param image_index Active swapchain image index.
     * @param frame Validated background and optional diagnostic raster.
     * @param raster_layout Layout present exactly when the frame has a raster.
     * @throws RendererError when command-buffer reset, begin, or end fails.
     */
    void record_frame_commands(
        const std::uint32_t image_index,
        const RenderFrame& frame,
        const std::optional<detail::DiagnosticRasterLayout>& raster_layout) {
        VkResult result = vkResetCommandBuffer(command_buffer_, 0U);
        if (result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::presentation_failed,
                               "Resetting the Vulkan command buffer", result);
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(command_buffer_, &begin_info);
        if (result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::presentation_failed,
                               "Beginning the Vulkan command buffer", result);
        }

        if (raster_layout.has_value()) {
            record_raster_clear(image_index, frame, raster_layout.value());
        } else {
            record_background_clear(image_index, frame.background);
        }

        result = vkEndCommandBuffer(command_buffer_);
        if (result != VK_SUCCESS) {
            throw vulkan_error(RendererErrorCode::presentation_failed,
                               "Ending the Vulkan command buffer", result);
        }
    }

    /**
     * @brief Populates backend-neutral device diagnostics after startup.
     */
    void update_diagnostics() {
        std::ostringstream vendor;
        vendor << "vendor 0x" << std::hex << std::uppercase << physical_device_properties_.vendorID;
        diagnostics_ = {
            .backend = RendererBackend::vulkan,
            .api_major = VK_API_VERSION_MAJOR(physical_device_properties_.apiVersion),
            .api_minor = VK_API_VERSION_MINOR(physical_device_properties_.apiVersion),
            .api_patch = VK_API_VERSION_PATCH(physical_device_properties_.apiVersion),
            .profile = "not-applicable",
            .debug_diagnostics = validation_enabled_,
            .api_version = "loader " + version_text(loader_api_version_) + ", device "
                           + version_text(physical_device_properties_.apiVersion),
            .vendor = vendor.str(),
            .device = physical_device_properties_.deviceName,
            .presentation = {},
            .presented_frames = 0U,
            .debug_error_count = validation_state_.error_count.load(std::memory_order_relaxed),
        };
        update_presentation_diagnostics();
    }

    /**
     * @brief Refreshes the neutral swapchain description after recreation.
     */
    void update_presentation_diagnostics() {
        if (swapchain_.handle == VK_NULL_HANDLE) {
            diagnostics_.presentation = "deferred zero-extent Vulkan swapchain";
            return;
        }
        diagnostics_.presentation =
            std::string{format_name(swapchain_.format)}
            + " + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR + VK_PRESENT_MODE_FIFO_KHR, "
            + std::to_string(swapchain_.extent.width) + 'x'
            + std::to_string(swapchain_.extent.height)
            + ", images=" + std::to_string(swapchain_.images.size())
            + ", graphics-queue=" + std::to_string(queue_selection_.graphics_family)
            + ", present-queue=" + std::to_string(queue_selection_.presentation_family)
            + ", transfer-clear + dynamic-rendering raster";
    }

    /**
     * @brief Converts accumulated validation errors into a typed failure.
     * @param code Failure category appropriate to the current lifecycle phase.
     * @param operation Diagnostic phase name.
     * @throws RendererError when enabled validation has reported any error.
     */
    void throw_if_validation_errors(const RendererErrorCode code,
                                    const std::string_view operation) const {
        if (!validation_enabled_) {
            return;
        }
        const std::uint64_t count = validation_state_.error_count.load(std::memory_order_relaxed);
        if (count != 0U) {
            throw RendererError{code, RendererBackend::vulkan,
                                std::string{operation} + " observed " + std::to_string(count)
                                    + " validation error message(s)"};
        }
    }

    /**
     * @brief Releases all native resources in strict reverse dependency order.
     */
    void cleanup_noexcept() noexcept {
        if (device_ != VK_NULL_HANDLE) {
            static_cast<void>(vkDeviceWaitIdle(device_));
            destroy_swapchain_state(swapchain_);
            if (in_flight_fence_ != VK_NULL_HANDLE) {
                vkDestroyFence(device_, in_flight_fence_, nullptr);
                in_flight_fence_ = VK_NULL_HANDLE;
            }
            if (image_available_semaphore_ != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_, image_available_semaphore_, nullptr);
                image_available_semaphore_ = VK_NULL_HANDLE;
            }
            if (command_pool_ != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device_, command_pool_, nullptr);
                command_pool_ = VK_NULL_HANDLE;
                command_buffer_ = VK_NULL_HANDLE;
            }
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
            graphics_queue_ = VK_NULL_HANDLE;
            presentation_queue_ = VK_NULL_HANDLE;
        }

        if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
            SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }
        if (debug_messenger_ != VK_NULL_HANDLE && destroy_debug_messenger_ != nullptr
            && instance_ != VK_NULL_HANDLE) {
            destroy_debug_messenger_(instance_, debug_messenger_, nullptr);
            debug_messenger_ = VK_NULL_HANDLE;
        }
        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
    }

    /** Native SDL window borrowed from the containing Application. */
    SDL_Window& window_;

    /** Validation policy fixed by the creating factory. */
    VulkanRendererConfiguration configuration_;

    /** Thread on which all SDL/Vulkan lifecycle and frame calls are permitted. */
    std::thread::id creator_thread_;

    /** Current lifecycle state exposed through the noexcept observer. */
    std::atomic<RendererLifecycleState> state_{RendererLifecycleState::dormant};

    /** Thread-safe validation callback evidence. */
    ValidationState validation_state_{};

    /** Whether the validation layer and debug messenger are active. */
    bool validation_enabled_{};

    /** Whether the selected device requires the portability-subset extension. */
    bool portability_subset_enabled_{};

    /** Whether swapchain replacement is required before the next frame. */
    bool swapchain_dirty_{};

    /** Actual version advertised by the Vulkan loader. */
    std::uint32_t loader_api_version_{VK_API_VERSION_1_0};

    /** Vulkan instance owning the surface and logical device relationship. */
    VkInstance instance_{VK_NULL_HANDLE};

    /** Optional validation/debug messenger owned by the instance. */
    VkDebugUtilsMessengerEXT debug_messenger_{VK_NULL_HANDLE};

    /** Loaded debug-utils messenger creation function. */
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger_{};

    /** Loaded debug-utils messenger destruction function. */
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger_{};

    /** SDL-created presentation surface owned until before window destruction. */
    VkSurfaceKHR surface_{VK_NULL_HANDLE};

    /** Selected physical device borrowed from the instance. */
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};

    /** Selected physical-device facts retained for diagnostics. */
    VkPhysicalDeviceProperties physical_device_properties_{};

    /** Deterministically selected graphics and presentation families. */
    vulkan_detail::QueueSelection queue_selection_{};

    /** Logical device owning all frame resources. */
    VkDevice device_{VK_NULL_HANDLE};

    /** Borrowed queue used for transfer and dynamic-rendering submission. */
    VkQueue graphics_queue_{VK_NULL_HANDLE};

    /** Borrowed queue used for swapchain presentation. */
    VkQueue presentation_queue_{VK_NULL_HANDLE};

    /** Resettable command pool for the one frame in flight. */
    VkCommandPool command_pool_{VK_NULL_HANDLE};

    /** Single primary command buffer reused after its fence signals. */
    VkCommandBuffer command_buffer_{VK_NULL_HANDLE};

    /** Binary semaphore signalled by swapchain acquisition. */
    VkSemaphore image_available_semaphore_{VK_NULL_HANDLE};

    /** Fence proving that the sole submitted frame has completed. */
    VkFence in_flight_fence_{VK_NULL_HANDLE};

    /** Transactionally replaceable swapchain generation. */
    SwapchainState swapchain_{};

    /** Backend-neutral facts retained while the renderer is running. */
    RendererDiagnostics diagnostics_{};
};

/**
 * @brief Factory for the concrete SDL/Vulkan 1.3 renderer backend.
 */
class VulkanRendererFactory final : public RendererFactory {
public:
    /**
     * @brief Stores the validation policy used by created renderers.
     * @param configuration Vulkan renderer configuration.
     */
    explicit VulkanRendererFactory(const VulkanRendererConfiguration configuration) noexcept
        : configuration_(configuration) {}

    /** @copydoc game_ex::render::RendererFactory::backend */
    [[nodiscard]] RendererBackend backend() const noexcept override {
        return RendererBackend::vulkan;
    }

    /** @copydoc game_ex::render::RendererFactory::required_window_api */
    [[nodiscard]] platform::WindowGraphicsApi required_window_api() const noexcept override {
        return platform::WindowGraphicsApi::vulkan;
    }

    /** @copydoc game_ex::render::RendererFactory::create */
    [[nodiscard]] std::unique_ptr<Renderer> create(platform::Window& window) const override {
        SDL_Window* const native_window =
            platform::sdl_detail::native_window(window, platform::WindowGraphicsApi::vulkan);
        if (native_window == nullptr) {
            throw RendererError{RendererErrorCode::incompatible_window, RendererBackend::vulkan,
                                "Vulkan renderer requires an SDL window created with "
                                "Vulkan capability"};
        }
        return std::make_unique<VulkanRenderer>(*native_window, configuration_);
    }

private:
    /** Validation policy copied into each returned renderer. */
    VulkanRendererConfiguration configuration_;
};

} // namespace

std::unique_ptr<RendererFactory>
create_vulkan_renderer_factory(const VulkanRendererConfiguration configuration) {
    return std::make_unique<VulkanRendererFactory>(configuration);
}

} // namespace game_ex::render
