#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>

/**
 * @brief Central logging system for the engine.
 *
 * Wraps spdlog and provides:
 * - Console logging
 * - File logging
 * - SDL log redirection
 *
 * This system must be initialized once at application startup
 * before any logging macros are used.
 */
class Log
{
public:
    /**
     * @brief Initializes the logging system.
     *
     * Creates console and file sinks and configures
     * log level depending on build type.
     *
     * Must be called once before using LOG_* macros.
     */
	static void Init();

    /**
     * @brief Returns the underlying spdlog logger instance.
     *
     * @return Shared pointer to the logger.
     */
	inline static std::shared_ptr<spdlog::logger>& GetLogger()
	{
		return s_Logger;
	}

private:
    /// Global logger instance
	static std::shared_ptr<spdlog::logger> s_Logger;
};

/// Logging macros
#define LOG_TRACE(...)    Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    Log::GetLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    Log::GetLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Log::GetLogger()->critical(__VA_ARGS__)
