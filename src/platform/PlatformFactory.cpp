#include "IWindow.h"
#include "sdl/SdlWindow.h"
#include <memory>
#include <iostream>

std::unique_ptr<platform::IWindow> CreatePlatformWindow()
{
	auto window = std::make_unique<platform::SdlWindow>();
	if (!window->initialize("Game_EX", 800, 600))
	{
		std::cerr << "Failed to initialize SDL window" << std::endl;
		return nullptr;
	}
	return window;
}