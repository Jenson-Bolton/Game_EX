#include <filesystem>

#include "engine/core/log/Log.h"

#include "game/HelloTriangleGame.h"

int main()
{
	// --- Logging ---
	std::filesystem::create_directories("logs");

	Log::Init();


	LOG_INFO("Game starting...");
	LOG_DEBUG("Debug logging enabled");

	// sdl etc...

	//HelloTriangle::run();
}
