#include "SdlWindow.h"
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>
#include <iostream>

namespace platform
{
	SdlWindow::SdlWindow()
	{
		// SDL initialization moved to factory function
	}

	SdlWindow::~SdlWindow()
	{
		if (m_window)
		{
			SDL_DestroyWindow(m_window);
		}
	}

	bool SdlWindow::initialize(const char* title, int width, int height)
	{
		// Set SDL hints for macOS
		SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
		
		m_window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN);
		if (!m_window)
		{
			const char* error = SDL_GetError();
			std::cerr << "SDL_CreateWindow failed: '" << (error ? error : "NULL") << "'" << std::endl;
			return false;
		}
		return true;
	}

	void SdlWindow::pollEvents()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				m_shouldClose = true;
			}
		}
	}

	bool SdlWindow::shouldClose() const
	{
		return m_shouldClose;
	}

	void SdlWindow::getFramebufferSize(uint32_t& outWidth, uint32_t& outHeight) const
	{
		int width, height;
		SDL_GetWindowSizeInPixels(m_window, &width, &height);
		outWidth = static_cast<uint32_t>(width);
		outHeight = static_cast<uint32_t>(height);
	}

	IWindow::NativeWindowHandle SdlWindow::getNativeHandle() const
	{
		NativeWindowHandle handle;
		handle.platformWindow = m_window;
		// For macOS, we don't need platformDisplay for Vulkan
		return handle;
	}
}
