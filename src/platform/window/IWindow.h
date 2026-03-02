#pragma once
#include <cstdint>

/**
 * @file IWindow.h
 * @brief Window inteface and native-handle abstraction.
 */

namespace platform::window
{
	/**
	 * @brief Configuration used when creating a window.
	 */
    struct WindowDesc {
        int width = 1280;               ///< Requested logical window width in pixels.
        int height = 720;               ///< Requested logical window height in pixels.
        const char* title = "Game_EX";  ///< Initial window title.
        bool resizable = true;          ///< If true, the user can resize the window.
    };

    /**
     * @brief Identifies the kind of native windowing handle returned by a platform implementation.
     *
     * Notes:
     * - Some platforms require both a "display" and a "handle/surface".
     * - Web builds often do not expose meaningful native handles.
     */
    enum class NativeHandleType {
        Unknown,
        Win32,
        X11,
        Wayland,
        Cocoa,
        Web,
        Sdl
    };

    /**
     * @brief Opaque container for native window handles.
     *
     * Typical interpretations:
     * - Win32:  handle = HWND, display = HINSTANCE (optional)
     * - X11:    handle = Window (integer ID cast to void*), display = Display*
     * - Wayland:handle = wl_surface*, display = wl_display*
     * - Cocoa:  handle = NSWindow*, display = nullptr
     * - SDL:    handle = SDL_Window* (fallback), display = nullptr
     */
    struct NativeHandle {
        NativeHandleType type = NativeHandleType::Unknown;
        void* handle = nullptr;
        void* display = nullptr;
    };

    /**
 * @brief Platform-agnostic window interface.
 *
 * The engine and renderer depend only on this interface, not SDL/Win32/Cocoa.
 * Implementations translate OS events into engine-friendly state and/or events.
 */
    class IWindow {
    public:
        virtual ~IWindow() = default;

        /**
         * @brief Create the window.
         * @param desc Window configuration.
         * @return true on success, false on failure.
         */
        virtual bool create(const WindowDesc& desc) = 0;

        /**
         * @brief Destroy the window and release underlying platform resources.
         */
        virtual void destroy() = 0;

        /**
         * @brief Pump the OS event loop and update internal state.
         *
         * Call once per frame (or more) to keep the application responsive.
         */
        virtual void poll_events() = 0;

        /**
         * @brief Indicates the user requested the app to close.
         * @return true if the window/application should close.
         */
        virtual bool should_close() const = 0;

        /**
         * @brief Get the drawable/backbuffer size in pixels.
         *
         * On HiDPI systems (e.g., macOS retina), this may differ from window size.
         * Renderers typically use this for swapchain/backbuffer sizing.
         *
         * @param width  Output framebuffer width.
         * @param height Output framebuffer height.
         */
        virtual void get_framebuffer_size(int& width, int& height) const = 0;

        /**
         * @brief Get the logical window size in pixels.
         * @param width  Output window width.
         * @param height Output window height.
         */
        virtual void get_window_size(int& width, int& height) const = 0;

        /**
         * @brief True if the window is minimized (not drawable).
         */
        virtual bool is_minimized() const = 0;

        /**
         * @brief Set the window title.
         * @param title New title (nullptr treated as empty string).
         */
        virtual void set_title(const char* title) = 0;

        /**
         * @brief Get native platform handles needed by low-level backends.
         *
         * Backends (e.g., Vulkan) may require these to create surfaces.
         *
         * @return A NativeHandle struct describing the underlying window handle(s).
         */
        virtual NativeHandle get_native_handle() const = 0;

        /**
         * @brief Whether a resize was observed since the last poll.
         * @return A bool if the window was resized
         */
        virtual bool was_resized() const = 0;

        /**
         * @brief Clear the resize flag.
         */
        virtual void clear_resized_flag() = 0;
    };
} 