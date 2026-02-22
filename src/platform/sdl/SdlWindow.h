#pragma once

#include <SDL3/SDL.h>
#include "../IWindow.h"

namespace platform
{
	class SdlWindow : public IWindow
	{
	public:
		SdlWindow();
		~SdlWindow() override;

		bool initialize(const char* title, int width, int height);
		void pollEvents() override;
		bool shouldClose() const override;
		void getFramebufferSize(uint32_t& outWidth, uint32_t& outHeight) const override;
		NativeWindowHandle getNativeHandle() const override;

	private:
		SDL_Window* m_window = nullptr;
		bool m_shouldClose = false;
	};
}
