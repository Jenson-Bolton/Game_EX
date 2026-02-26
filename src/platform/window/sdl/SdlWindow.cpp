// SdlWindow.cpp
#include "platform/window/sdl/SdlWindow.h"

#include <cstdint> // uintptr_t
#include <SDL3/SDL.h>

namespace platform::window {

    static SDL_WindowFlags build_sdl_window_flags(const WindowDesc& desc) {
        SDL_WindowFlags flags = 0;

#if defined(GAMEX_RENDERER_VULKAN)
        flags = (SDL_WindowFlags)(flags | SDL_WINDOW_VULKAN);
#endif

        if (desc.resizable) {
            flags = (SDL_WindowFlags)(flags | SDL_WINDOW_RESIZABLE);
        }

        // HiDPI / retina friendly pixel sizing
        flags = (SDL_WindowFlags)(flags | SDL_WINDOW_HIGH_PIXEL_DENSITY);

        return flags;
    }

    bool SdlWindow::create(const WindowDesc& desc) {
        if (window_) {
            return true;
        }

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            // TODO: log SDL_GetError()
            return false;
        }

        window_ = SDL_CreateWindow(desc.title, desc.width, desc.height, build_sdl_window_flags(desc));
        if (!window_) {
            // TODO: log SDL_GetError()
            SDL_Quit();
            return false;
        }

        should_close_ = false;
        minimized_ = false;
        resized_ = false;

        get_window_size(cached_win_w_, cached_win_h_);
        get_framebuffer_size(cached_fb_w_, cached_fb_h_);

        return true;
    }

    void SdlWindow::destroy() {
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        SDL_Quit();

        should_close_ = false;
        minimized_ = false;
        resized_ = false;
    }

    void SdlWindow::poll_events() {
        resized_ = false;

        SDL_Event e{};
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT:
                should_close_ = true;
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                minimized_ = true;
                break;

            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_SHOWN:
            case SDL_EVENT_WINDOW_EXPOSED:
                minimized_ = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                // SDL3 provides size data on the window event.
                // For safety (and HiDPI correctness) we also refresh via queries below.
                cached_win_w_ = e.window.data1;
                cached_win_h_ = e.window.data2;

                get_framebuffer_size(cached_fb_w_, cached_fb_h_);
                resized_ = true;
            } break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                should_close_ = true;
                break;

            default:
                break;
            }

            // TODO: forward input events to your input system here.
        }
    }

    void SdlWindow::get_framebuffer_size(int& width, int& height) const {
        width = 0;
        height = 0;
        if (!window_) return;

        // SDL3: correct pixel size for swapchains/backbuffers on HiDPI systems
        SDL_GetWindowSizeInPixels(window_, &width, &height);
    }

    void SdlWindow::get_window_size(int& width, int& height) const {
        width = 0;
        height = 0;
        if (!window_) return;

        SDL_GetWindowSize(window_, &width, &height);
    }

    void SdlWindow::set_title(const char* title) {
        if (!window_) return;
        SDL_SetWindowTitle(window_, title ? title : "");
    }

    NativeHandle SdlWindow::get_native_handle() const {
        NativeHandle out{};
        if (!window_) return out;

        const SDL_PropertiesID props = SDL_GetWindowProperties(window_);

#if defined(_WIN32)
        out.type = NativeHandleType::Win32;
        out.handle = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        out.display = nullptr;
        return out;

#elif defined(__APPLE__)
        out.type = NativeHandleType::Cocoa;
        out.handle = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        out.display = nullptr;
        return out;

#elif defined(__EMSCRIPTEN__)
        out.type = NativeHandleType::Web;
        out.handle = nullptr;
        out.display = nullptr;
        return out;

#elif defined(__linux__)
        // Prefer Wayland if present
        void* wl_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        void* wl_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        if (wl_display && wl_surface) {
            out.type = NativeHandleType::Wayland;
            out.display = wl_display;
            out.handle = wl_surface;
            return out;
        }

        void* x11_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        const Sint64 x11_window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (x11_display && x11_window != 0) {
            out.type = NativeHandleType::X11;
            out.display = x11_display;
            out.handle = (void*)(uintptr_t)x11_window; // X11 Window is an integer id
            return out;
        }

        // Unknown backend: fall back to SDL_Window*
        out.type = NativeHandleType::Sdl;
        out.handle = window_;
        out.display = nullptr;
        return out;

#else
        // Fallback: expose SDL_Window* for backends that can use SDL APIs directly
        out.type = NativeHandleType::Sdl;
        out.handle = window_;
        out.display = nullptr;
        return out;
#endif
    }

} // namespace platform::window