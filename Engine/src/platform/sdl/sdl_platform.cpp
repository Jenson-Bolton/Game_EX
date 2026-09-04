/**
 * @file sdl_platform.cpp
 * @brief SDL3 platform and window implementation.
 */

#include "game_ex/platform/sdl_platform.hpp"

#include <SDL3/SDL.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace game_ex::platform {
namespace {

/**
 * @brief Converts an SDL failure into an exception with the current SDL detail.
 * @param operation Human-readable description of the failed operation.
 * @return Runtime error containing operation and SDL error text.
 */
[[nodiscard]] std::runtime_error sdl_error(const std::string& operation) {
    return std::runtime_error(operation + ": " + SDL_GetError());
}

/**
 * @brief SDL3-backed top-level window with unique native ownership.
 */
class SdlWindow final : public Window {
public:
    /**
     * @brief Adopts a successfully created SDL window.
     * @param window Native window; must not be null.
     */
    explicit SdlWindow(SDL_Window* window) noexcept : window_(window) {}

    /** Destroys the native window before the SDL video subsystem shuts down. */
    ~SdlWindow() override {
        SDL_DestroyWindow(window_);
    }

    /** @copydoc Window::show */
    void show() override {
        if (!SDL_ShowWindow(window_)) {
            throw sdl_error("SDL could not show the window");
        }
    }

    /** @copydoc Window::hide */
    void hide() override {
        if (!SDL_HideWindow(window_)) {
            throw sdl_error("SDL could not hide the window");
        }
    }

private:
    /** Native SDL window owned by this adapter. */
    SDL_Window* window_;
};

/**
 * @brief SDL3-backed process platform runtime.
 *
 * This foundation supports one top-level window per runtime. The restriction is
 * explicit so multi-window editor requirements can be specified before a wider
 * event-routing API is introduced.
 */
class SdlPlatform final : public Platform {
public:
    /**
     * @brief Reports application metadata and initialises SDL video/events.
     * @param specification Metadata validated by the public factory.
     */
    explicit SdlPlatform(const SdlPlatformSpecification& specification) {
        if (!SDL_SetAppMetadata(
                specification.application_name.c_str(),
                specification.application_version.c_str(),
                specification.application_identifier.c_str())) {
            throw sdl_error("SDL could not set application metadata");
        }

        if (!SDL_Init(SDL_INIT_VIDEO)) {
            auto error = sdl_error("SDL could not initialise the video subsystem");
            SDL_Quit();
            throw error;
        }
    }

    /** Shuts down SDL after every window owned by Application is destroyed. */
    ~SdlPlatform() override {
        SDL_Quit();
    }

    /** @copydoc Platform::create_window */
    [[nodiscard]] std::unique_ptr<Window> create_window(
        const WindowSpecification& specification) override {
        if (window_created_) {
            throw std::logic_error("The foundation SDL platform supports one window per process");
        }

        if (specification.title.empty()) {
            throw std::invalid_argument("Window title cannot be empty");
        }

        constexpr auto maximum_dimension = static_cast<std::uint32_t>(
            std::numeric_limits<int>::max());
        if (specification.width == 0 || specification.height == 0
            || specification.width > maximum_dimension
            || specification.height > maximum_dimension) {
            throw std::invalid_argument("Window dimensions must be positive SDL-compatible integers");
        }

        SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
        if (specification.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        SDL_Window* native_window = SDL_CreateWindow(
            specification.title.c_str(),
            static_cast<int>(specification.width),
            static_cast<int>(specification.height),
            flags);
        if (native_window == nullptr) {
            throw sdl_error("SDL could not create the window");
        }

        window_created_ = true;
        return std::make_unique<SdlWindow>(native_window);
    }

    /** @copydoc Platform::pump_events */
    [[nodiscard]] EventPumpResult pump_events() override {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT
                || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                return EventPumpResult::exit_requested;
            }
        }

        return EventPumpResult::continue_running;
    }

private:
    /** Prevents ambiguous event ownership until multi-window support is specified. */
    bool window_created_{false};
};

} // namespace

std::unique_ptr<Platform> create_sdl_platform(
    const SdlPlatformSpecification& specification) {
    if (specification.application_name.empty()
        || specification.application_version.empty()
        || specification.application_identifier.empty()) {
        throw std::invalid_argument("SDL application metadata fields cannot be empty");
    }

    return std::make_unique<SdlPlatform>(specification);
}

} // namespace game_ex::platform
