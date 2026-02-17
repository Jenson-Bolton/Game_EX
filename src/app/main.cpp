//#include "core/log/Log.h"

#include <filesystem>

int main()
{
	// --- Logging ---
	//std::filesystem::create_directories("logs");

	//Log::Init();

	//LOG_INFO("Game starting...");
	//LOG_DEBUG("Debug logging enabled");

	// sdl etc...
    
    // --------------- Temp Demo -------------

    // 1) Platform window (SDL implementation behind IWindow)
    std::unique_ptr<IWindow> window = CreatePlatformWindow(/* config */);
    if (!window) return -1;

    // 2) Renderer backend (Vulkan implementation behind IRenderer)
    std::unique_ptr<IRenderer> renderer = CreateRendererBackend(/* config */);
    if (!renderer) return -1;

    IRenderer::InitInfo rinfo{};
    if (!renderer->initialize(rinfo, *window))
        return -1;

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
    return 0;

}
