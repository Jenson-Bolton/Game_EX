#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <SDL3/SDL.h>

std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::Init()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_pattern("[%T] [%^%l%$] %v");

	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/game.log", true);
	file_sink->set_pattern("[%T] [%^%l%$] %v");

	std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };

	s_Logger = std::make_shared<spdlog::logger>("GAME", sink.begin(), sinks.end());
	spdlog::register_logger(s_Logger);

#ifdef NDEBUG
	s_Logger->set_level(spdlog::level::info);
#else
	s_Logger->set_level(spdlog::level::trace);
#endif
}

void SDLLogCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
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

SDL_LogSetOutputFunction(SDLLogCallback, nullptr);
