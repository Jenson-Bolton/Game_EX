#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>


/**
 * @file Log.h
 * @brief spglog implmentation.
 */

namespace engine::core
{
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
	    static void init();

        /**
         * @brief Returns the underlying spdlog logger instance.
         *
         * @return Shared pointer to the logger.
         */
	    inline static std::shared_ptr<spdlog::logger>& get_logger()
	    {
		    return logger_;
	    }

    private:
        /// Global logger instance
	    static std::shared_ptr<spdlog::logger> logger_;
    };
} // namespace engine::core

/// Logging macros
#define LOG_TRACE(...)    engine::core::Log::get_logger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    engine::core::Log::get_logger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     engine::core::Log::get_logger()->info(__VA_ARGS__)
#define LOG_WARN(...)     engine::core::Log::get_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    engine::core::Log::get_logger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) engine::core::Log::get_logger()->critical(__VA_ARGS__)
