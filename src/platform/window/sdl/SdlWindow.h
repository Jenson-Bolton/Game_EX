#pragma once

#include "platform/window/IWindow.h"

/**
 * @file SdlWindow.h
 * @brief SDL-based implementation of platform::window::IWindow.
 */

struct SDL_Window; ///< forward declaration; SDL headers are included in .cpp

namespace platform::window
{
	/**
	 * @brief SDL window implementation.
	 *
	 * Owns an SDL_Window and translates SDL events into the IWindow state queried by the engine.
	 */
	class SdlWindow final : public IWindow
	{
	public:
        SdlWindow() = default;
        ~SdlWindow() override { destroy(); }

        bool create(const WindowDesc& desc) override;
        void destroy() override;

        void poll_events() override;
        bool should_close() const override { return should_close_; }

        void get_framebuffer_size(int& width, int& height) const override;
        void get_window_size(int& width, int& height) const override;

        bool is_minimized() const override { return minimized_; }

        void set_title(const char* title) override;

        NativeHandle get_native_handle() const override;

        /**
         * @brief Whether a resize was observed since the last poll.
         */
        bool was_resized() const { return resized_; }

        /**
         * @brief Clear the resize flag.
         */
        void clear_resized_flag() { resized_ = false; }

    private:
        SDL_Window* window_ = nullptr;

        bool should_close_ = false;
        bool minimized_ = false;
        bool resized_ = false;

        int cached_fb_w_ = 0;
        int cached_fb_h_ = 0;
        int cached_win_w_ = 0;
        int cached_win_h_ = 0;
	};
}
