/**
 * @file opengl_renderer.cpp
 * @brief SDL/OpenGL 4.6 Core diagnostic renderer implementation.
 */

#include "game_ex/render/opengl_renderer.hpp"

#include "platform/sdl/sdl_window_access.hpp"
#include "render/diagnostic_raster_layout.hpp"

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace game_ex::render {
namespace {

/** OpenGL baseline required by this backend. */
constexpr std::uint32_t required_major_version{4U};

/** OpenGL baseline required by this backend. */
constexpr std::uint32_t required_minor_version{6U};

/**
 * @brief Returns one OpenGL procedure through the current SDL context.
 * @param name Null-terminated OpenGL procedure name supplied by GLAD.
 * @return Procedure address, or null when the driver does not expose it.
 */
[[nodiscard]] GLADapiproc load_open_gl_procedure(const char* const name) noexcept {
    return SDL_GL_GetProcAddress(name);
}

/**
 * @brief Checks every loaded OpenGL procedure invoked by this renderer slice.
 * @return True when diagnostics, sRGB, viewport, scissor, clear, and error calls are safe.
 */
[[nodiscard]] bool required_open_gl_procedures_loaded() noexcept {
    return glad_glGetError != nullptr
        && glad_glGetIntegerv != nullptr
        && glad_glGetString != nullptr
        && glad_glDisable != nullptr
        && glad_glEnable != nullptr
        && glad_glIsEnabled != nullptr
        && glad_glScissor != nullptr
        && glad_glViewport != nullptr
        && glad_glClearColor != nullptr
        && glad_glClear != nullptr;
}

/**
 * @brief Builds a renderer error with the current SDL diagnostic.
 * @param code Stable renderer failure category.
 * @param operation Human-readable failed operation.
 * @return Categorized exception ready to throw.
 */
[[nodiscard]] RendererError current_sdl_error(
    const RendererErrorCode code,
    const std::string& operation) {
    return RendererError{
        code,
        RendererBackend::open_gl,
        operation + ": " + SDL_GetError()};
}

/**
 * @brief Converts the current lifecycle state to stable diagnostic text.
 * @param state Lifecycle state to describe.
 * @return Static lowercase state name.
 */
[[nodiscard]] std::string_view lifecycle_state_name(
    const RendererLifecycleState state) noexcept {
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
 * @brief Converts a GL error value into a compact hexadecimal diagnostic.
 * @param value Error returned by glGetError().
 * @return Human-readable hexadecimal representation.
 */
[[nodiscard]] std::string open_gl_error_text(const GLenum value) {
    std::ostringstream stream;
    stream << "OpenGL error 0x" << std::hex << static_cast<unsigned int>(value);
    return stream.str();
}

/**
 * @brief SDL-backed OpenGL 4.6 Core renderer with unique context ownership.
 */
class OpenGlRenderer final : public Renderer {
public:
    /**
     * @brief Binds a dormant renderer to a checked SDL OpenGL window.
     * @param window Native SDL window owned by the containing Application.
     */
    explicit OpenGlRenderer(SDL_Window& window) noexcept
        : window_(window), creator_thread_(std::this_thread::get_id()) {}

    /**
     * @brief Releases a remaining context, failing fast on wrong-thread destruction.
     *
     * Normal callers use shutdown(). This fallback exists for exceptions and for
     * partial starts. SDL context destruction is thread-affine, so silently
     * cleaning up on another thread would be unsafe.
     */
    ~OpenGlRenderer() override {
        if (std::this_thread::get_id() != creator_thread_) {
            std::terminate();
        }

        destroy_context_noexcept();
    }

    /** @copydoc game_ex::render::Renderer::backend */
    [[nodiscard]] RendererBackend backend() const noexcept override {
        return RendererBackend::open_gl;
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
            context_ = SDL_GL_CreateContext(&window_);
            if (context_ == nullptr) {
                throw current_sdl_error(
                    RendererErrorCode::unavailable,
                    "SDL could not create an OpenGL 4.6 Core context");
            }

            if (!SDL_GL_MakeCurrent(&window_, context_)) {
                throw current_sdl_error(
                    RendererErrorCode::initialization_failed,
                    "SDL could not make the OpenGL context current");
            }

            const int loaded_version = gladLoadGL(load_open_gl_procedure);
            if (loaded_version == 0 || GLAD_GL_VERSION_4_6 == 0) {
                throw RendererError{
                    RendererErrorCode::unavailable,
                    RendererBackend::open_gl,
                    "GLAD did not load a context advertising OpenGL 4.6 Core"};
            }

            if (!required_open_gl_procedures_loaded()) {
                throw RendererError{
                    RendererErrorCode::initialization_failed,
                    RendererBackend::open_gl,
                    "GLAD did not resolve every OpenGL procedure used by the diagnostic renderer"};
            }

            while (glGetError() != GL_NO_ERROR) {
                // Discard loader probing errors before querying required facts.
            }

            GLint actual_major{};
            GLint actual_minor{};
            GLint actual_profile{};
            GLint actual_flags{};
            int srgb_capable{};
            int double_buffered{};
            glGetIntegerv(GL_MAJOR_VERSION, &actual_major);
            glGetIntegerv(GL_MINOR_VERSION, &actual_minor);
            glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &actual_profile);
            glGetIntegerv(GL_CONTEXT_FLAGS, &actual_flags);

            if (!SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &srgb_capable)) {
                throw current_sdl_error(
                    RendererErrorCode::initialization_failed,
                    "SDL could not query the OpenGL framebuffer colour space");
            }
            if (!SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &double_buffered)) {
                throw current_sdl_error(
                    RendererErrorCode::initialization_failed,
                    "SDL could not query OpenGL double-buffer availability");
            }

            const auto* const version_text = glGetString(GL_VERSION);
            const auto* const vendor_text = glGetString(GL_VENDOR);
            const auto* const device_text = glGetString(GL_RENDERER);
            const GLenum query_error = glGetError();
            if (query_error != GL_NO_ERROR) {
                throw RendererError{
                    RendererErrorCode::initialization_failed,
                    RendererBackend::open_gl,
                    "OpenGL context diagnostics failed: " + open_gl_error_text(query_error)};
            }

            if (actual_major < 0 || actual_minor < 0) {
                throw RendererError{
                    RendererErrorCode::initialization_failed,
                    RendererBackend::open_gl,
                    "OpenGL returned an invalid negative context version"};
            }

            const auto major = static_cast<std::uint32_t>(actual_major);
            const auto minor = static_cast<std::uint32_t>(actual_minor);
            if (major < required_major_version
                || (major == required_major_version && minor < required_minor_version)) {
                throw RendererError{
                    RendererErrorCode::unavailable,
                    RendererBackend::open_gl,
                    "The created context does not meet the OpenGL 4.6 baseline"};
            }

            if ((actual_profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
                throw RendererError{
                    RendererErrorCode::unavailable,
                    RendererBackend::open_gl,
                    "The created OpenGL context is not a Core profile context"};
            }

            if (srgb_capable == 0) {
                throw RendererError{
                    RendererErrorCode::unavailable,
                    RendererBackend::open_gl,
                    "The OpenGL default framebuffer is not sRGB-capable"};
            }
            if (double_buffered == 0) {
                throw RendererError{
                    RendererErrorCode::unavailable,
                    RendererBackend::open_gl,
                    "The OpenGL default framebuffer is not double-buffered"};
            }

            if (version_text == nullptr || vendor_text == nullptr || device_text == nullptr) {
                throw RendererError{
                    RendererErrorCode::initialization_failed,
                    RendererBackend::open_gl,
                    "OpenGL did not provide required context diagnostic strings"};
            }

            glEnable(GL_FRAMEBUFFER_SRGB);
            if (glIsEnabled(GL_FRAMEBUFFER_SRGB) != GL_TRUE) {
                throw RendererError{
                    RendererErrorCode::initialization_failed,
                    RendererBackend::open_gl,
                    "OpenGL could not enable linear-to-sRGB framebuffer conversion"};
            }

            const GLenum colour_space_error = glGetError();
            if (colour_space_error != GL_NO_ERROR) {
                throw RendererError{
                    RendererErrorCode::initialization_failed,
                    RendererBackend::open_gl,
                    "OpenGL sRGB framebuffer setup failed: "
                        + open_gl_error_text(colour_space_error)};
            }

            diagnostics_ = {
                .backend = RendererBackend::open_gl,
                .api_major = major,
                .api_minor = minor,
                .api_patch = 0U,
                .profile = "core",
                .debug_diagnostics = (actual_flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0,
                .api_version = reinterpret_cast<const char*>(version_text),
                .vendor = reinterpret_cast<const char*>(vendor_text),
                .device = reinterpret_cast<const char*>(device_text),
                .presentation = "sRGB double-buffered default framebuffer",
                .presented_frames = 0U,
                .debug_error_count = 0U,
            };
            state_.store(RendererLifecycleState::running, std::memory_order_release);
        } catch (...) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            throw;
        }
    }

    /** @copydoc game_ex::render::Renderer::render_frame */
    [[nodiscard]] FramePresentationResult render_frame(
        const RenderFrame& frame) override {
        require_creator_thread("render a frame");
        require_state(RendererLifecycleState::running, "render a frame");
        validate_render_frame(frame);

        try {
            if (!SDL_GL_MakeCurrent(&window_, context_)) {
                throw current_sdl_error(
                    RendererErrorCode::presentation_failed,
                    "SDL could not make the OpenGL context current for presentation");
            }

            int pixel_width{};
            int pixel_height{};
            if (!SDL_GetWindowSizeInPixels(&window_, &pixel_width, &pixel_height)) {
                throw current_sdl_error(
                    RendererErrorCode::presentation_failed,
                    "SDL could not query the window drawable size in pixels");
            }

            if (pixel_width < 0 || pixel_height < 0) {
                throw RendererError{
                    RendererErrorCode::presentation_failed,
                    RendererBackend::open_gl,
                    "SDL returned a negative drawable pixel extent"};
            }
            if (pixel_width == 0 || pixel_height == 0) {
                return FramePresentationResult::deferred_zero_extent;
            }

            glViewport(0, 0, pixel_width, pixel_height);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(
                frame.background.red,
                frame.background.green,
                frame.background.blue,
                frame.background.alpha);
            glClear(GL_COLOR_BUFFER_BIT);

            if (frame.raster.has_value()) {
                const DiagnosticRaster& raster = frame.raster.value();
                const detail::DiagnosticRasterLayout layout =
                    detail::layout_diagnostic_raster(
                        raster,
                        static_cast<std::uint32_t>(pixel_width),
                        static_cast<std::uint32_t>(pixel_height));

                glEnable(GL_SCISSOR_TEST);
                for (const detail::DiagnosticRasterRectangle& rectangle : layout.cells) {
                    const DiagnosticFrame& colour =
                        raster.linear_colours[rectangle.colour_index];
                    glScissor(
                        static_cast<GLint>(rectangle.x),
                        static_cast<GLint>(rectangle.y),
                        static_cast<GLsizei>(rectangle.width),
                        static_cast<GLsizei>(rectangle.height));
                    glClearColor(colour.red, colour.green, colour.blue, colour.alpha);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                glDisable(GL_SCISSOR_TEST);
            }

            const GLboolean scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
            const GLenum frame_error = glGetError();
            if (frame_error != GL_NO_ERROR || scissor_enabled != GL_FALSE) {
                throw RendererError{
                    RendererErrorCode::presentation_failed,
                    RendererBackend::open_gl,
                    frame_error != GL_NO_ERROR
                        ? "OpenGL diagnostic raster clear failed: "
                            + open_gl_error_text(frame_error)
                        : "OpenGL diagnostic raster did not restore scissor state"};
            }

            if (!SDL_GL_SwapWindow(&window_)) {
                throw current_sdl_error(
                    RendererErrorCode::presentation_failed,
                    "SDL could not present the OpenGL diagnostic frame");
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
            throw RendererError{
                RendererErrorCode::invalid_state,
                RendererBackend::open_gl,
                "Cannot shut down an OpenGL renderer in state "
                    + std::string{lifecycle_state_name(current)}};
        }

        state_.store(RendererLifecycleState::stopping, std::memory_order_release);
        if (context_ != nullptr && !SDL_GL_DestroyContext(context_)) {
            state_.store(RendererLifecycleState::failed, std::memory_order_release);
            throw current_sdl_error(
                RendererErrorCode::shutdown_failed,
                "SDL could not destroy the OpenGL context");
        }

        context_ = nullptr;
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
            throw RendererError{
                RendererErrorCode::invalid_state,
                RendererBackend::open_gl,
                "OpenGL renderer may only " + std::string{operation}
                    + " on its creator thread"};
        }
    }

    /**
     * @brief Requires one exact lifecycle state.
     * @param expected State required by the operation.
     * @param operation Human-readable operation used in the exception.
     * @throws RendererError when the current state differs.
     */
    void require_state(
        const RendererLifecycleState expected,
        const std::string_view operation) const {
        const RendererLifecycleState current = state();
        if (current != expected) {
            throw RendererError{
                RendererErrorCode::invalid_state,
                RendererBackend::open_gl,
                "Cannot " + std::string{operation} + " an OpenGL renderer in state "
                    + std::string{lifecycle_state_name(current)}};
        }
    }

    /**
     * @brief Best-effort context cleanup used only during object destruction.
     */
    void destroy_context_noexcept() noexcept {
        if (context_ != nullptr) {
            static_cast<void>(SDL_GL_DestroyContext(context_));
            context_ = nullptr;
        }
    }

    /** Native SDL window borrowed from the containing Application. */
    SDL_Window& window_;

    /** Thread on which SDL/OpenGL lifecycle calls are permitted. */
    std::thread::id creator_thread_;

    /** Current lifecycle state exposed through the noexcept observer. */
    std::atomic<RendererLifecycleState> state_{RendererLifecycleState::dormant};

    /** Native OpenGL context, retained during a partial-start failure for rollback. */
    SDL_GLContext context_{nullptr};

    /** Driver facts retained while the renderer is running. */
    RendererDiagnostics diagnostics_{};
};

/**
 * @brief Factory for the OpenGL implementation of the shared renderer contract.
 */
class OpenGlRendererFactory final : public RendererFactory {
public:
    /** @copydoc game_ex::render::RendererFactory::backend */
    [[nodiscard]] RendererBackend backend() const noexcept override {
        return RendererBackend::open_gl;
    }

    /** @copydoc game_ex::render::RendererFactory::required_window_api */
    [[nodiscard]] platform::WindowGraphicsApi required_window_api() const noexcept override {
        return platform::WindowGraphicsApi::open_gl;
    }

    /** @copydoc game_ex::render::RendererFactory::create */
    [[nodiscard]] std::unique_ptr<Renderer> create(platform::Window& window) const override {
        SDL_Window* const native_window = platform::sdl_detail::native_window(
            window,
            platform::WindowGraphicsApi::open_gl);
        if (native_window == nullptr) {
            throw RendererError{
                RendererErrorCode::incompatible_window,
                RendererBackend::open_gl,
                "OpenGL renderer requires an SDL window created with OpenGL capability"};
        }

        return std::make_unique<OpenGlRenderer>(*native_window);
    }
};

} // namespace

std::unique_ptr<RendererFactory> create_opengl_renderer_factory() {
    return std::make_unique<OpenGlRendererFactory>();
}

} // namespace game_ex::render
