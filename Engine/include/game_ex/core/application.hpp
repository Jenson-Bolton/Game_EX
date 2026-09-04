/**
 * @file application.hpp
 * @brief Minimal engine application lifecycle.
 */

#pragma once

#include "game_ex/platform/platform.hpp"

#include <chrono>
#include <memory>
#include <optional>

namespace game_ex::core {

/**
 * @brief Configures behaviour of the foundation-stage application loop.
 * @ingroup core
 */
struct RunConfiguration final {
    /**
     * Optional lifetime used by automated smoke tests. A normal application
     * leaves this empty and runs until the user closes its window.
     */
    std::optional<std::chrono::milliseconds> automatic_exit_after;

    /** Sleep duration used while the foundation has no update/render workload. */
    std::chrono::milliseconds idle_sleep{8};
};

/**
 * @brief Owns the platform runtime and drives the top-level application loop.
 *
 * Application is the first composition boundary, not a service locator. The
 * executable selects a concrete Platform implementation and transfers ownership
 * to this object. A future startup graph will be hosted above this minimal boot
 * sequence once its specification has been agreed.
 *
 * @ingroup core
 */
class Application final {
public:
    /**
     * @brief Creates the application's single foundation-stage window.
     * @param platform Initialised platform runtime selected by the composition root.
     * @param window Requested window properties.
     * @param run Main-loop timing and optional smoke-test lifetime.
     * @throws std::invalid_argument if platform is null or run is invalid.
     * @throws std::runtime_error if the platform cannot create the window.
     */
    Application(
        std::unique_ptr<platform::Platform> platform,
        const platform::WindowSpecification& window,
        RunConfiguration run = {});

    /** Releases the window before releasing its owning platform runtime. */
    ~Application();

    /** Applications have unique ownership of platform resources. */
    Application(const Application&) = delete;

    /** Applications cannot be copy-assigned. */
    Application& operator=(const Application&) = delete;

    /**
     * @brief Shows the window and processes events until shutdown is requested.
     * @return Zero after an orderly shutdown.
     * @throws std::runtime_error if the platform cannot show the native window.
     */
    int run();

private:
    /** Platform runtime; declared before window so it is destroyed after it. */
    std::unique_ptr<platform::Platform> platform_;

    /** The current milestone's sole top-level application window. */
    std::unique_ptr<platform::Window> window_;

    /** Main-loop configuration copied at construction. */
    RunConfiguration run_configuration_;
};

} // namespace game_ex::core
