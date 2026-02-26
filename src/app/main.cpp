#include <filesystem>

#include "platform/window/sdl/SdlWindow.h"
#include "engine/core/log/Log.h"


int main(int /*argc*/, char** /*argv*/)
{
	// --- Logging ---
	std::filesystem::create_directories("logs");

	engine::core::Log::init();
	LOG_INFO("Game starting...");
	LOG_DEBUG("Debug logging enabled");
	LOG_TRACE("Trace Logging enabled");

	platform::window::WindowDesc wd{};
	wd.width = 1280;
	wd.height = 720;
	wd.title = "Game_EX";
	wd.resizable = true;

	platform::window::SdlWindow window;
	if (!window.create(wd)) {
		LOG_CRITICAL("Failed to create window!");
		return 1;
	}

	// auto renderer = create_renderer(); // std::unique_ptr<IRenderer>
	// if (!renderer || !renderer->initialize(window)) return 1;

	while (!window.should_close())
	{
		window.poll_events();

		if (window.is_minimized()) {
			// sleep, no render, etc. save power
			continue;
		}

		if (window.was_resized()) {
			int fb_w = 0, fb_h = 0;
			window.get_framebuffer_size(fb_w, fb_h);

			if (fb_w > 0 && fb_h > 0) {
				// renderer->resize((uint32_t)fb_w, (uint32_t)fb_h);
			}

			window.clear_resized_flag();

			// renderer->begin_frame();
			// update_game(dt);
			// renderer->end_frame();
		}
	}

	// renderer->shutdown();
	window.destroy();
	return 0;

}
