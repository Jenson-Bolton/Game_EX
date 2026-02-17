#pragma once
#include <cstdint>

namespace platform
{
	/**
	 * @brief Window Interface.
	 * 
	 * Independeant window system.
	 * Render backends can query native handles through @ref NativeWindowHandle
	 * to create API-specific surfaces/swapchains.
	 */
	class IWindow
	{
	public:
		/**
		 * @brief Native handle bundle for active platform backend.
		 */
		struct NativeWindowHandle
		{
			void* platformWindow  = nullptr; ///< E.g. SDL_Window*, HWND, NSWindow*
			void* platformDisplay = nullptr; ///< E.g. X11 Display*, Wayland display, etc.
		};

		virtual ~IWindow() = default;

		/**
		 * @brief Poll window/input events and update internal state.
		 */
		virtual void pollEvents() = 0;

		/**
		 * @brief Whether the window requested to close.
		 */
		virtual bool shouldClose() const = 0;

		/**
		 * @brief Get drawable framebuffer size in pixels (not logical points).
		 */
		virtual void getFramebufferSize(uint32_t& outWidth, uint32_t& outHeight) const = 0;

		/**
		 * @brief Get platform native handles needed by render backends.
		 */
		virtual NativeWindowHandle getNativeHandle() const = 0;
	};
}
