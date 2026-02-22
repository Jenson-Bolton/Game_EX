#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

std::shared_ptr<spdlog::logger> Log::s_Logger;

/**
 * @brief SDL log output callback.
 *
 * Redirects SDL logging into the engine logging system.
 *
 * @param userdata User data pointer (unused).
 * @param category SDL log category.
 * @param priority SDL log priority.
 * @param message Log message string.
 */
static void SDLCALL SDLLogCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
	switch (priority)
	{
	case SDL_LOG_PRIORITY_ERROR:
		LOG_ERROR("[SDL] {}", message);
		break;
	case SDL_LOG_PRIORITY_WARN:
		LOG_WARN("[SDL] {}", message);
		break;
	case SDL_LOG_PRIORITY_INFO:
		LOG_INFO("[SDL] {}", message);
		break;
	default:
		LOG_DEBUG("[SDL] {}", message);
		break;
	}
}

void Log::Init()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_pattern("[%T] [%^%l%$] %v");

	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/game.log", true);
	file_sink->set_pattern("[%T] [%^%l%$] %v");

	std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };

	s_Logger = std::make_shared<spdlog::logger>("GAME", sinks.begin(), sinks.end());
	spdlog::register_logger(s_Logger);

#ifdef NDEBUG
	s_Logger->set_level(spdlog::level::info);
#else
	s_Logger->set_level(spdlog::level::trace);
#endif

	// Redirect SDL logging into engine logger
	SDL_SetLogOutputFunction(SDLLogCallback, nullptr);
}
