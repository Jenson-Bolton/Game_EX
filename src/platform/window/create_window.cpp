#include "platform/window/create_window.h"

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
#include "platform/window/sdl/SdlWindow.h"
#else
#error [window] Unsupported platform
#endif

namespace platform::window
{
	std::unique_ptr<IWindow> create_window(const WindowDesc& wd) {
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
		auto window = std::make_unique<SdlWindow>();
		if (!window->create(wd)) return {};
		return window;

#else
		static_assert(true, "No window implementation for selected platform");
#endif
	}
}