/**
 * @file platform.hpp
 * @brief Low-level platform capability interface.
 */

#pragma once

#include "game_ex/platform/window.hpp"

#include <memory>

namespace game_ex::platform {

/**
 * @brief Result of pumping the operating system event queue.
 * @ingroup platform
 */
enum class EventPumpResult {
    /** The application should continue its main loop. */
    continue_running,

    /** The user or operating system requested application shutdown. */
    exit_requested
};

/**
 * @brief Abstracts the small set of operating-system services needed at boot.
 *
 * This interface deliberately exposes low-level capabilities rather than game
 * concepts. Save games, assets, rendering, and simulation remain responsibilities
 * of higher engine or game modules.
 *
 * @ingroup platform
 */
class Platform {
public:
    /** Enables destruction through the platform-neutral interface. */
    virtual ~Platform() = default;

    /** Platform runtimes are uniquely owned because they control process state. */
    Platform(const Platform&) = delete;

    /** Platform runtimes cannot be copy-assigned. */
    Platform& operator=(const Platform&) = delete;

    /**
     * @brief Creates one hidden top-level window.
     * @param specification Requested title, size, and behaviour.
     * @return A uniquely owned platform-neutral window.
     * @throws std::invalid_argument if the specification is invalid.
     * @throws std::logic_error if the backend cannot own another window.
     * @throws std::runtime_error if native window creation fails.
     */
    [[nodiscard]] virtual std::unique_ptr<Window> create_window(
        const WindowSpecification& specification) = 0;

    /**
     * @brief Processes all currently queued operating-system events.
     * @return Whether the application should keep running.
     */
    [[nodiscard]] virtual EventPumpResult pump_events() = 0;

protected:
    /** Allows construction only by concrete platform implementations. */
    Platform() = default;
};

} // namespace game_ex::platform
