/**
 * @file renderer.hpp
 * @brief Backend-neutral diagnostic-frame rendering contract.
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace game_ex::render {

/**
 * @brief Identifies a concrete graphics API used by a renderer.
 * @ingroup render
 */
enum class RendererBackend {
    /** OpenGL renderer backend. */
    open_gl,

    /** Vulkan 1.3 renderer backend. */
    vulkan
};

/**
 * @brief Converts a backend value to stable human-readable text.
 * @param backend Backend to describe.
 * @return Static lowercase backend name suitable for logs and CLI values.
 * @ingroup render
 */
[[nodiscard]] std::string_view renderer_backend_name(RendererBackend backend) noexcept;

/**
 * @brief Observable lifecycle state of an owned renderer.
 * @ingroup render
 */
enum class RendererLifecycleState {
    /** Constructed but not yet started. */
    dormant,

    /** A start operation is in progress. */
    starting,

    /** Ready to render and present frames. */
    running,

    /** A shutdown operation is in progress. */
    stopping,

    /** Shutdown completed and the renderer cannot be restarted. */
    stopped,

    /** Startup, native frame work, or shutdown failed; cleanup may remain. */
    failed
};

/**
 * @brief Stable category attached to a backend-neutral renderer exception.
 * @ingroup render
 */
enum class RendererErrorCode {
    /** An operation was not valid in the current lifecycle state. */
    invalid_state,

    /** The supplied platform window cannot host this renderer. */
    incompatible_window,

    /** The requested graphics API or required version is unavailable. */
    unavailable,

    /** Renderer startup failed after the API was selected. */
    initialization_failed,

    /** A frame could not be cleared or presented. */
    presentation_failed,

    /** Renderer cleanup could not complete normally. */
    shutdown_failed
};

/**
 * @brief Backend-neutral exception carrying a stable failure category.
 * @ingroup render
 */
class RendererError final : public std::runtime_error {
public:
    /**
     * @brief Creates a categorized renderer failure.
     * @param code Stable failure category.
     * @param backend Backend that reported the failure.
     * @param message Human-readable diagnostic detail.
     */
    RendererError(RendererErrorCode code, RendererBackend backend, std::string message);

    /**
     * @brief Returns the stable failure category.
     * @return Category supplied at construction.
     */
    [[nodiscard]] RendererErrorCode code() const noexcept;

    /**
     * @brief Returns the backend that reported the failure.
     * @return Backend supplied at construction.
     */
    [[nodiscard]] RendererBackend backend() const noexcept;

private:
    /** Stable failure category. */
    RendererErrorCode code_;

    /** Backend that reported the failure. */
    RendererBackend backend_;
};

/**
 * @brief One clear colour for the diagnostic renderer slice.
 *
 * Components are linear floating-point values in the inclusive range [0, 1].
 * This release intentionally exposes no shaders, meshes, command buffers, or
 * speculative resource API.
 *
 * @ingroup render
 */
struct DiagnosticFrame final {
    /** Red clear component in [0, 1]. */
    float red{};

    /** Green clear component in [0, 1]. */
    float green{};

    /** Blue clear component in [0, 1]. */
    float blue{};

    /** Alpha clear component in [0, 1]. */
    float alpha{1.0F};
};

/**
 * @brief Observable outcome of one backend-neutral presentation request.
 * @ingroup render
 */
enum class FramePresentationResult {
    /** The frame was submitted to the native presentation system. */
    presented,

    /** Presentation was safely deferred while the drawable pixel extent was zero. */
    deferred_zero_extent,

    /** Presentation was deferred while the native surface changed. */
    deferred_surface_change
};

/**
 * @brief Returns the shared game/editor diagnostic clear frame.
 * @return Finite, validated dark-blue clear colour.
 * @ingroup render
 */
[[nodiscard]] DiagnosticFrame foundation_diagnostic_frame() noexcept;

/**
 * @brief Validates the backend-neutral diagnostic frame contract.
 * @param frame Frame whose four clear components are checked.
 * @throws std::invalid_argument if any component is non-finite or outside [0, 1].
 * @ingroup render
 */
void validate_diagnostic_frame(const DiagnosticFrame& frame);

/**
 * @brief Runtime facts reported by a successfully started renderer.
 * @ingroup render
 */
struct RendererDiagnostics final {
    /** Concrete backend that produced these facts. */
    RendererBackend backend{RendererBackend::open_gl};

    /** Actual graphics API major version. */
    std::uint32_t api_major{};

    /** Actual graphics API minor version. */
    std::uint32_t api_minor{};

    /** Actual graphics API patch version, or zero when the API does not expose one. */
    std::uint32_t api_patch{};

    /** API profile text, or `not-applicable` when the backend has no profile. */
    std::string profile;

    /** Whether API-level debug diagnostics were requested and enabled. */
    bool debug_diagnostics{};

    /** Driver-supplied graphics API version text. */
    std::string api_version;

    /** Driver-supplied vendor text. */
    std::string vendor;

    /** Driver-supplied device or renderer text. */
    std::string device;

    /** Backend-neutral description of the active presentation configuration. */
    std::string presentation;

    /** Number of frames successfully handed to native presentation. */
    std::uint64_t presented_frames{};

    /** Number of API validation errors observed by enabled debug diagnostics. */
    std::uint64_t debug_error_count{};
};

/**
 * @brief Owns one concrete graphics context and its presentation lifecycle.
 *
 * A renderer is an ordinary uniquely owned object. The composition thread must
 * call start(), render_frame(), diagnostics(), shutdown(), and destroy the
 * object. A failed start or native frame remains shutdown-capable so
 * StartupGraph can release partial or uncertain native state. A stopped
 * renderer is intentionally not restartable.
 *
 * @ingroup render
 */
class Renderer {
public:
    /** Enables cleanup through the backend-neutral ownership interface. */
    virtual ~Renderer() = default;

    /** Renderers uniquely own graphics and presentation state. */
    Renderer(const Renderer&) = delete;

    /** Renderers cannot be copy-assigned. */
    Renderer& operator=(const Renderer&) = delete;

    /**
     * @brief Identifies this object's concrete backend.
     * @return Concrete backend selected by its factory.
     */
    [[nodiscard]] virtual RendererBackend backend() const noexcept = 0;

    /**
     * @brief Reports the current lifecycle state without throwing.
     * @return Current renderer lifecycle state.
     */
    [[nodiscard]] virtual RendererLifecycleState state() const noexcept = 0;

    /**
     * @brief Creates and validates graphics-API state for the bound window.
     * @throws RendererError if state, compatibility, availability, or startup fails.
     * @throws std::bad_alloc if diagnostic storage cannot be allocated.
     */
    virtual void start() = 0;

    /**
     * @brief Clears the drawable pixel area and presents one diagnostic frame.
     * @param frame Validated clear colour shared by game and editor composition.
     * @return Presented, deferred at zero extent, or deferred for a surface change.
     * @throws std::invalid_argument if frame components violate the public contract.
     * @throws RendererError if lifecycle, drawable query, API work, or swap fails.
     *
     * Invalid input and a typed deferral leave the renderer running. Once native
     * frame work fails, the backend enters `failed`; callers must shut it down and
     * must not retry presentation on that object.
     */
    [[nodiscard]] virtual FramePresentationResult render_frame(
        const DiagnosticFrame& frame) = 0;

    /**
     * @brief Returns facts queried from the active graphics context.
     * @return Reference valid until this renderer is destroyed.
     * @throws RendererError unless the renderer is currently running.
     */
    [[nodiscard]] virtual const RendererDiagnostics& diagnostics() const = 0;

    /**
     * @brief Releases native graphics state after its window has been hidden.
     *
     * Shutdown accepts running and failed states, enabling rollback of partial
     * startup and terminal native-frame state. It rejects dormant, stopping,
     * and already-stopped states.
     *
     * @throws RendererError if lifecycle or native cleanup fails.
     */
    virtual void shutdown() = 0;

protected:
    /** Allows construction only by concrete renderer implementations. */
    Renderer() = default;
};

} // namespace game_ex::render
