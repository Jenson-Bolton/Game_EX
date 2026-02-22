//#include "core/log/Log.h"

#include "../platform/IWindow.h"
#include "../engine/renderer/IRenderer.h"
#include "../game/HelloTriangleGame.h"
#include <SDL3/SDL.h>
#include <iostream>

using namespace platform;

// Forward declarations for factory functions
std::unique_ptr<IWindow> CreatePlatformWindow();
std::unique_ptr<IRenderer> CreateRendererBackend();

int main()
{
	// Initialize SDL
	std::cout << "Initializing SDL..." << std::endl;
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
		return -1;
	}
	std::cout << "SDL initialized successfully" << std::endl;

	// --- Logging ---
	//std::filesystem::create_directories("logs");

	//Log::Init();

	//LOG_INFO("Game starting...");
	//LOG_DEBUG("Debug logging enabled");

	// sdl etc...
    
    // --------------- Temp Demo -------------

    // 1) Platform window (SDL implementation behind IWindow)
    std::unique_ptr<IWindow> window = CreatePlatformWindow(/* config */);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        return -1;
    }

    // 2) Renderer backend (Vulkan implementation behind IRenderer)
    std::unique_ptr<IRenderer> renderer = CreateRendererBackend(/* config */);
    if (!renderer) {
        std::cerr << "Failed to create renderer" << std::endl;
        return -1;
    }

    IRenderer::InitInfo rinfo{};
    if (!renderer->initialize(rinfo, *window)) {
        std::cerr << "Failed to initialize renderer backend" << std::endl;
        return -1;
    }

    // 3) Game instance
    HelloTriangleGame game;
    if (!game.initialize(*renderer))
        return -1;

    // 4) Main loop (keep your existing style)
    while (!window->shouldClose())
    {
        window->pollEvents();
        game.tick(*renderer);
    }

    game.shutdown(*renderer);
    renderer->shutdown();
    
    SDL_Quit();
    return 0;

}
