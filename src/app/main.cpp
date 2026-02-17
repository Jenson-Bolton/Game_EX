#include "core/log/Log.h"

#include <filesystem>

int main()
{
	// --- Logging ---
	std::filesystem::create_directories("logs");

	Log::Init();

	LOG_INFO("Game starting...");
	LOG_DEBUG("Debug logging enabled");

	// sdl etc...
}
